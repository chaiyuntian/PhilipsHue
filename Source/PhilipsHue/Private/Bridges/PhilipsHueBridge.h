// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhilipsHueBridgeMessages.h"
#include "PhilipsHueLight.h"
#include "PhilipsHueBridge.generated.h"


/** Multicast delegate that is invoked when a bridge has been connected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhilipsHueBridgeConnected, FString, BridgeId);

/** Multicast delegate that is invoked when a bridge has been disconnected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhilipsHueBridgeDisconnected, FString, BridgeId);

/** Multicast delegate that is invoked when a username acquired from local cache has been passed the test or a new user was authorized. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhilipsHueUserAuthorized, FString, UserName);


/** Multicast delegate that is invoked when a username is not authorized and remind that push button need to be pressed before new user creation*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhilipsHueBridgePushLinkRequested, FString, BridgeId);


/**
 * A Philips Hue bridge.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class UPhilipsHueBridge : public UObject
{
	GENERATED_BODY()

	/** Default constructor. */
	UPhilipsHueBridge();

	/** Destructor. */
	~UPhilipsHueBridge();

public:

	/** Bridge configuration. */
	UPROPERTY(BlueprintReadOnly, Category="PhilipsHue|Bridge")
	FPhilipsHueConfiguration Configuration;

	/** Unique bridge identifier. */
	UPROPERTY(BlueprintReadOnly, Category="PhilipsHue|Bridge")
	FString Id;

public:

	/**
	 * Connect the bridge.
	 *
	 * @param User The user account identifier to authenticate with.
	 */
	UFUNCTION(BlueprintCallable, Category="PhilipsHue|Bridge")
		void Connect(const FString& User);

	/** Disconnect the bridge. */
	UFUNCTION(BlueprintCallable, Category="PhilipsHue|Bridge")
		void Disconnect();

	/*
	* Input a FPhilipsHueLightSetState to set the state of of one light
	*
	* @param GroupID
	* @param newState FPhilipsHueLightSetState value
	*/
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightStateByID(int32 LightID, FPhilipsHueLightSetState newState);

	/*
	 * Input a FPhilipsHueLightSetState to set the state of a group of lights
	 *
	 * @param GroupID
	 * @param newState FPhilipsHueLightSetState value
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightGroupActionByID(const int32 GroupID, const FPhilipsHueLightSetState newState);


	/*
	 * Input a raw json to set the state of a group of lights
	 *
	 * @param GroupID 
	 * @param StateJson raw Json string
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightGroupActionByIDRaw(const int32 GroupID, const FString StateJson);


	/*
	* Set brightness of a group of lights
	*
	* @param GroupID Group identifier
	* @param Brightness  range from 0.0 ~ 1.0
	*/
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightBrightnessByGroupID(const int32 GroupID, const float Brightness);


	/*
	* Set the color of a group of lights
	*
	* @param GroupID Group identifier
	* @param InColor color will be converted to xy value inside the function
	*/
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightColorByGroupID(const int32 GroupID, const FLinearColor InColor);

	/*
	 * Input a raw json to set the state of one light
	 *
	 * @param LightID
	 * @param StateJson raw Json string
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightStateByLightIDRaw(const int32 LightID, const FString StateJson);

	/*
	 * Set brightness of one light
	 *
	 * @param LightID Light identifier
	 * @param Brightness  range from 0.0 ~ 1.0
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightBrightnessByLightID(const int32 LightID, const float Brightness);


	/*
	 * Set the color of one light
	 *
	 * @param LightID Light identifier
	 * @param InColor color will be converted to xy value inside the function
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void SetLightColorByLightID(const int32 LightID, const FLinearColor InColor);


	/*
	 * Get the user id either from file or by creation
	 *
	 * @param FromFile if true, the user id will be get from the local file or it will create a new user after pushing link button
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		void AquireUserID(bool FromFile = true);
	
	/*
	 * Get user identifier
	 */
	UFUNCTION(BlueprintCallable, Category = "PhilipsHue|Bridge")
		FString  UserID();

public:

	/** A delegate that is invoked when a bridge has been connected. */
	UPROPERTY(BlueprintAssignable, Category="PhilipsHue|Bridge")
		FOnPhilipsHueBridgeConnected OnConnected;

	/** A delegate that is invoked when a bridge has been disconnected. */
	UPROPERTY(BlueprintAssignable, Category="PhilipsHue|Bridge")
		FOnPhilipsHueBridgeDisconnected OnDisconnected;

	/** A delegate that is invoked when a user id validation has finished*/
	UPROPERTY(BlueprintAssignable, Category = "PhilipsHue|Bridge")
		FOnPhilipsHueUserAuthorized OnHueUserAuthorized;

	/** A delegate that remind the user id is not authorized*/
	UPROPERTY(BlueprintAssignable, Category = "PhilipsHue|Bridge")
		FOnPhilipsHueBridgePushLinkRequested OnPushLinkRequested;

private:

	/** Handles the completion of HTTP requests. */
	void HandleHttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/** Handles the completion of User ID query*/
	void HandleUserIDRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/** Handles the User ID validation, if the user id is acquired from local file*/
	void HandleUserIDTestRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/** Handles the Light/Group State Change*/
	void HandleLightStateRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/** Handles the ticker. */
	bool HandleTicker(float DeltaTime);

	/** Make a user file path*/
	FString HueUserFile();

	/* Get the user id either from file */
	bool GetUserIDFromLocalFile();

	/* After the user has been authorized, store the user id in a local file*/
	void SaveUserIDToFile();

private:

	/** Whether the bridge is connected. */
	bool Connected;

	/** The identifier of the connect user account. */
	FString ConnectedUser;

	/** Handle to the registered ticker. */
	FDelegateHandle TickerHandle;
};
