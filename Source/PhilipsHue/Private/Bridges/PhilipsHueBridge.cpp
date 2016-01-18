// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PrivatePCH.h"
#include "Ticker.h"


/* UPhilipsHueBridge structors
 *****************************************************************************/

UPhilipsHueBridge::UPhilipsHueBridge()
	: Connected(false)
{

}

UPhilipsHueBridge::~UPhilipsHueBridge()
{
	Disconnect();
}


/* Inner utils
*****************************************************************************/
/* color to xy conversion */

FLinearColor xyToColor(FVector2D xy)
{
	float x = xy.X;
	float y = xy.Y;

	if (y == 0){ return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f); }

	float z = 1.0f - x - y;
	float Y = 1.0f;
	float X = (Y / y) * x;
	float Z = (Y / y) * z;

	// sRGB D65 conversion
	float r = X  * 3.2406f - Y * 1.5372f - Z * 0.4986f;
	float g = -X * 0.9689f + Y * 1.8758f + Z * 0.0415f;
	float b = X  * 0.0557f - Y * 0.2040f + Z * 1.0570f;

	if (r > b && r > g && r > 1.0f) {
		// red is too big
		g = g / r;
		b = b / r;
		r = 1.0f;
	}
	else if (g > b && g > r && g > 1.0f) {
		// green is too big
		r = r / g;
		b = b / g;
		g = 1.0f;
	}
	else if (b > r && b > g && b > 1.0f) {
		// blue is too big
		r = r / b;
		g = g / b;
		b = 1.0f;
	}

	// Apply gamma correction
	r = r <= 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * pow(r, (1.0f / 2.4f)) - 0.055f;
	g = g <= 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * pow(g, (1.0f / 2.4f)) - 0.055f;
	b = b <= 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * pow(b, (1.0f / 2.4f)) - 0.055f;

	if (r > b && r > g) {
		// red is biggest
		if (r > 1.0f) {
			g = g / r;
			b = b / r;
			r = 1.0f;
		}
	}
	else if (g > b && g > r) {
		// green is biggest
		if (g > 1.0f) {
			r = r / g;
			b = b / g;
			g = 1.0f;
		}
	}
	else if (b > r && b > g) {
		// blue is biggest
		if (b > 1.0f) {
			r = r / b;
			g = g / b;
			b = 1.0f;
		}
	}
	return FLinearColor(r, g, b, 1.0f);
}

FVector2D ColorToxy(FLinearColor LightColor)
{
	float red = LightColor.R;
	float green = LightColor.G;
	float blue = LightColor.B;

	// Apply gamma correction
	float r = (red   > 0.04045f) ? pow((red + 0.055f) / (1.0f + 0.055f), 2.4f) : (red / 12.92f);
	float g = (green > 0.04045f) ? pow((green + 0.055f) / (1.0f + 0.055f), 2.4f) : (green / 12.92f);
	float b = (blue  > 0.04045f) ? pow((blue + 0.055f) / (1.0f + 0.055f), 2.4f) : (blue / 12.92f);

	// Wide gamut conversion D65

	//float X = r * 0.649926f + g * 0.103455f + b * 0.197109f;
	//float Y = r * 0.234327f + g * 0.743075f + b * 0.022598f;
	//float Z = r * 0.0000000f + g * 0.053077f + b * 1.035763f;

	// SRGB
	float X = r * 0.4124f + g * 0.3576f + b * 0.1805f;
	float Y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
	float Z = r * 0.0193f + g * 0.1192f + b * 0.9502f;

	float cx = X / (X + Y + Z);
	float cy = Y / (X + Y + Z);

	if (isnan(cx)) {
		cx = 0.0f;
	}

	if (isnan(cy)) {
		cy = 0.0f;
	}
	return FVector2D(cx, cy);
}

FORCEINLINE FString UserRequestJson()
{
	FString JsonStr;
	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonStr);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("devicetype"), TEXT("pc#ue4hue"));
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	return JsonStr;
}

FORCEINLINE FString BuildJsonStrFromMap(TMap<FString, FString> Map)
{
	FString JsonStr;
	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonStr);
	// Close the writer and finalize the output such that JsonStr has what we want
	TArray<FString> keys;
	Map.GetKeys(keys);
	JsonWriter->WriteObjectStart();
	for (int32 k = 0; k < Map.Num(); k++)
	{
		JsonWriter->WriteValue(keys[k], Map[keys[k]]);
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	return JsonStr;
}


/* UPhilipsHueBridge interface
 *****************************************************************************/

void UPhilipsHueBridge::Connect(const FString& User)
{
	ConnectedUser = User;
	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPhilipsHueBridge::HandleTicker), 10.0f);
}

void UPhilipsHueBridge::AquireUserID(bool FromFile)
{

	if (FromFile&&GetUserIDFromLocalFile())
	{
		// Test if the User ID stored local is authorized
		auto HttpRequest1 = FHttpModule::Get().CreateRequest();
		{
			HttpRequest1->OnProcessRequestComplete().BindUObject(this, &UPhilipsHueBridge::HandleUserIDTestRequestComplete);
			HttpRequest1->SetURL(FString(TEXT("http://")) + Configuration.IpAddress + TEXT("/api/") + ConnectedUser);
			HttpRequest1->SetVerb(TEXT("GET"));
			HttpRequest1->ProcessRequest();
		}
		return;
	}

	auto HttpRequest = FHttpModule::Get().CreateRequest();
	{
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPhilipsHueBridge::HandleUserIDRequestComplete);
		HttpRequest->SetURL(FString(TEXT("http://")) + Configuration.IpAddress + TEXT("/api"));
		HttpRequest->SetHeader("Content-Type", "application/json");
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetContentAsString(UserRequestJson());
		HttpRequest->ProcessRequest();
	}
}

bool UPhilipsHueBridge::GetUserIDFromLocalFile()
{
	return FFileHelper::LoadFileToString(ConnectedUser, *HueUserFile());
}

FString UPhilipsHueBridge::HueUserFile()
{
	FString FolderDir = FPaths::Combine(*(FPaths::GameDir()), TEXT("hue"));
	return FPaths::Combine(*FolderDir, TEXT("hueuser"));
}

void UPhilipsHueBridge::SaveUserIDToFile()
{
	FFileHelper::SaveStringToFile(ConnectedUser, *HueUserFile());
}

void UPhilipsHueBridge::Disconnect()
{
	ConnectedUser.Empty();
	OnDisconnected.Broadcast(Id);
	FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

void UPhilipsHueBridge::SetLightStateByID(int32 LightID, FPhilipsHueLightSetState newState)
{
	/*TODO: Convert state to raw json and send*/
}

void UPhilipsHueBridge::SetLightGroupActionByID(const int32 GroupID, const  FPhilipsHueLightSetState newState)
{
	/*TODO: Convert state to raw json and send*/
}

void UPhilipsHueBridge::SetLightColorByGroupID(const int32 GroupID, const FLinearColor InColor)
{
	FVector2D xy = ColorToxy(InColor);
	FString JsonStr;
	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonStr);
	FString str = "[" + FString::SanitizeFloat(xy.X) + "," + FString::SanitizeFloat(xy.Y) + "]";
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteRawJSONValue(TEXT("xy"), str);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	SetLightGroupActionByIDRaw(0, JsonStr);
}

void UPhilipsHueBridge::SetLightBrightnessByGroupID(const int32 GroupID, const float Brightness)
{
	int32 Bri = (int32)(255.f*Brightness);
	FString JsonStr;
	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonStr);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("bri"), Bri);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	SetLightGroupActionByIDRaw(0, JsonStr);
}

void UPhilipsHueBridge::SetLightGroupActionByIDRaw(const int32 GroupID, const FString StateJson)
{
	if (!Connected)return;
	auto HttpRequest = FHttpModule::Get().CreateRequest();
	{
		FString GroupIDStr = FString::FromInt(GroupID);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPhilipsHueBridge::HandleLightStateRequestComplete);
		HttpRequest->SetURL(FString(TEXT("http://")) + Configuration.IpAddress + TEXT("/api/") + ConnectedUser + TEXT("/groups/") + GroupIDStr + TEXT("/action"));
		HttpRequest->SetHeader("Content-Type", "application/json");
		HttpRequest->SetVerb(TEXT("PUT"));
		HttpRequest->SetContentAsString(StateJson);
		HttpRequest->ProcessRequest();
	}
}

void UPhilipsHueBridge::SetLightStateByLightIDRaw(const int32 LightID, const FString StateJson)
{
	if (!Connected)return;
	auto HttpRequest = FHttpModule::Get().CreateRequest();
	{
		FString LightIDStr = FString::FromInt(LightID);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPhilipsHueBridge::HandleLightStateRequestComplete);
		HttpRequest->SetURL(FString(TEXT("http://")) + Configuration.IpAddress + TEXT("/api/") + ConnectedUser + TEXT("/lights/") + LightIDStr + TEXT("/state"));
		HttpRequest->SetHeader("Content-Type", "application/json");
		HttpRequest->SetVerb(TEXT("PUT"));
		HttpRequest->SetContentAsString(StateJson);
		HttpRequest->ProcessRequest();
	}
}

void UPhilipsHueBridge::SetLightBrightnessByLightID(const int32 LightID, const float Brightness)
{
	int32 Bri = (int32)(255.f*Brightness);
	FString JsonStr;
	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonStr);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("bri"), Bri);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	SetLightStateByLightIDRaw(LightID, JsonStr);

}

void UPhilipsHueBridge::SetLightColorByLightID(const int32 LightID, const FLinearColor InColor)
{
	FVector2D xy = ColorToxy(InColor);
	FString JsonStr;
	TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonStr);
	FString str = "[" + FString::SanitizeFloat(xy.X) + "," + FString::SanitizeFloat(xy.Y) + "]";
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteRawJSONValue(TEXT("xy"), str);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	SetLightStateByLightIDRaw(LightID, JsonStr);
}

FString  UPhilipsHueBridge::UserID()
{
	return ConnectedUser;
}


/* UPhilipsHueBridge event handlers
 *****************************************************************************/
void UPhilipsHueBridge::HandleHttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (!bSucceeded)
	{
		Disconnect();

		return;
	}

	if (!Connected)
	{
		Connected = true;
		OnConnected.Broadcast(Id);
	}

//	FString ResponseStr = HttpResponse->GetContentAsString();
}

bool UPhilipsHueBridge::HandleTicker(float DeltaTime)
{
	
	auto HttpRequest = FHttpModule::Get().CreateRequest();
	{
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPhilipsHueBridge::HandleHttpRequestComplete);
		HttpRequest->SetURL(FString(TEXT("http://")) + Configuration.IpAddress + TEXT("/api/") + ConnectedUser + TEXT("/lights"));
		HttpRequest->SetVerb(TEXT("GET"));
		HttpRequest->ProcessRequest();
	}

	return !ConnectedUser.IsEmpty();
}

void UPhilipsHueBridge::HandleLightStateRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	FString MessageBody = "";
	if (!HttpResponse.IsValid())
	{
		MessageBody = "{\"success\":\"Error: Unable to process HTTP Request!\"}";
	}
	else if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		MessageBody = HttpResponse->GetContentAsString();
		int32 idx = MessageBody.Find("error");
		if (idx > 0){
			//Send Error;
		}
		else
		{
			//Successfully sent!
		}

	}
	else
	{
		MessageBody = FString::Printf(TEXT("{\"success\":\"HTTP Error: %d\"}"), HttpResponse->GetResponseCode());
	}
}

void UPhilipsHueBridge::HandleUserIDTestRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	FString MessageBody = "";
	// If HTTP fails client-side, this will still be called but with a NULL shared pointer!
	if (!HttpResponse.IsValid())
	{
		MessageBody = "{\"success\":\"Error: Unable to process HTTP Request!\"}";
	}
	else if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		MessageBody = HttpResponse->GetContentAsString();
		int32 idx = MessageBody.Find("unauthorized user");
		if (idx > 0){
			//Remind push linking
			OnPushLinkRequested.Broadcast(Id);
			//AquireUserID(false);
		}
		else
		{
			Connected = true;
			// Successfully tested the user id
			OnHueUserAuthorized.Broadcast(ConnectedUser);
			
		}

	}
	else
	{
		MessageBody = FString::Printf(TEXT("{\"success\":\"HTTP Error: %d\"}"), HttpResponse->GetResponseCode());
	}

}

void UPhilipsHueBridge::HandleUserIDRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	FString MessageBody = "";
	FString userid = "";
	// If HTTP fails client-side, this will still be called but with a NULL shared pointer!
	if (!HttpResponse.IsValid())
	{
		MessageBody = "{\"success\":\"Error: Unable to process HTTP Request!\"}";
	}
	else if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
	{
		

		MessageBody = HttpResponse->GetContentAsString();

		TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(*MessageBody);
		int32 idx = MessageBody.Find("\"username\"");
		if (idx > 0){
			userid = MessageBody.Trim();
			userid.RemoveFromStart("[{\"success\":{\"username\":\"");
			userid.RemoveFromEnd("\"}}]");
			ConnectedUser = userid;
			SaveUserIDToFile();
			OnHueUserAuthorized.Broadcast(userid);
		}
		else
		{
			OnPushLinkRequested.Broadcast(Id);
		}
	}
	else
	{
		MessageBody = FString::Printf(TEXT("{\"success\":\"HTTP Error: %d\"}"), HttpResponse->GetResponseCode());
	}
}


