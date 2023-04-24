# Hitman Absolution & Sniper Challenge Server
This project emulates the server used by [Hitman: Absolution](https://store.steampowered.com/app/203140/Hitman_Absolution/) and the [Hitman: Sniper Challenge](https://steamcommunity.com/app/205930) and aims to restore all its features, including the Contracts feature.

# Getting started with the code
In order to get started with the code, you need to have one of the following IDEs/editors installed:
- Visual Studio 2022 (recommended)
- Jetbrains Rider
- Visual Studio Code with [vscode-solution-explorer](https://marketplace.visualstudio.com/items?itemName=fernandoescolar.vscode-solution-explorer) (recommended) and [C#](https://marketplace.visualstudio.com/items?itemName=ms-dotnettools.csharp) (required) installed.

On top of that, you need to have the [.NET SDK](https://dotnet.microsoft.com/en-us/download) for your OS installed (**not** the Runtime). The current project is based on .NET 6 LTS. You can however install any newer version you like.

## Getting starting with the hook
The dinput8.dll hook is based on C++. It's beyond the scope of this document to explain how to compile that.

Once you have a x32 bit compiled version however, just drop it in the game folder along with the `hook.ini` file, make changes accordingly and start the game.

The hook supports both Hitman Absolution and Hitman Sniper Challenge depending on what you set in the `hook.ini`. Both allow to change the WebService URL. Hitman Absolution additionally allows you to skip the launcher.

The hook does not *crack* the games in any sort of way, you still need a legitimate copy in order to make use of this project.

# TODO
In its current state, almost everything is reverse engineered and working with mocked data. But there are still a few things to figure out to restore the full experience.

## Server
Phase 1:
- ~~Figure out if game can actually POST a body~~ Nope! It can't.
- ~~Figure out if GetFeaturedContract is actually used~~ Yes! After loading a level in Singleplayer. Will be shown on the in-game menu.
- ~~Figure out if GetScoreComparison is acctually used~~ Yes! After loading a level in Singleplayer. Will be shown on the in-game HUD at the start of a level.
- ~~Figure out if __metadata is even needed anywhere~~ Nope! Doesn't seem like it at least.
- Figure out the MessageTemplateIds
- Add Unit Tests based on real requests made by the game.

Phase 2:
- Add EntityFramework and define a database schema
- Replace all mocked with actual functionality 

Phase 3:
- Expose leaderboards through Web UI

## Hook
- Figure out how to patch Hitman Sniper Challenge without triggering anti-debugging measures to skip launcher.
- IDEA: Hook Steam Friends API to hook up own account system with friends.

# Notes about Ids in-game
- UserId is almost always a 64-bit SteamId. However, to stay on the safe side all UserId-properties are treated as a String.
- ContractId is considered a String in-game, since `Play_01` is used by the Contracts tutorial. This also has a effect on the LeaderboardId-properties, since that property can be a ContractId if the game wants to receive Contract-specific leaderboards.
	- IDEA: Use the hook to change these ID's to a negative number so the data model can keep using Integers.

# Documentation
This section aims to explain the inner workings of the game in relation to the API endpoints that need to be reverse engineered.

## Startup Phase
The first interesting calls happen here:
- `ZOnlineManagerWindows::Update`
- `ZOnlineManager::Update`
	- Called near a `_SteamAPI_RunCallbacks`-call
	- Contains a lot of timed events that call API endpoints regularly

If the webservice is not connected yet, it will call `ZOSWebService::Connect` followed by ` OSuite::ZOnlineSuite::CreateWebServiceClient`. In this function the configured service-url is passed to `OSuite::ZWebServiceClientManager::Create`.

Since there is no instance for this webservice yet, it will go into `OSuite::ZWebServiceClient::Initialize`. This function registers the `os_getStatus` and `os_$metadata` endpoints with a callback to `OSuite::ZWebServiceClient::InternalProbeResultCallback`. Eventually, `OSuite::ZWebServiceClient::ProbeAvailability` is called.

`OSuite::ZWebServiceClient::ProbeAvailability` will prepare the request for the `os_getStatus` endpoint and use `OSuite::ZWebServiceClient::InternalProbeCallback` as a callback. This callback is responsible for parsing the response. 

If succesfully parsed, `OSuite::ZWebServiceClient::InternalProbeResultCallback` is called which will prepare the request to the `os_$metadata` endpoint with `OSuite::ZWebServiceClient::InternalMetadataCallback` as a callback.

Unlike the `os_getStatus` request, a cache is used for the metadata and `OSuite::ZAtomCache::Open<OSuite::ZOMetadata>` will be called to fetch the result from the endpoint. The response is parsed (see the notes about `OSuite::ZAtomCache::Open` below) and the `OSuite::ZWebServiceClient::InternalMetadataCallback` callback is called. 

This function will check if a valid Metadata-response was given through `OSuite::ZWebServiceClient::RetrieveRequest<OSuite::ZOMetadata>` and if there was, flag the ZWebServiceClient as connected (`m_eStatus = 2 //READY_STATE`)

## OSuite::ZAtomCache::Open
Something very important happens in any `OSuite::ZAtomCache::Open<T>`-function, as it will make an instance of a `TAtomObject<T>`. When the response comes back from the endpoint, this instance will get its `Read`-function called, which will then make an instance of the generic type.

There are a few of these `Read`-functions responsible for creating instances for:
- `OSuite::ZOEntry`
- `OSuite::ZOFeed`
- `OSuite::ZOMetadata`
- `OSuite::ZOServiceOperationResult`

Any of these constructors will call their respective `ParseJsonValue`-function.

## Invoking requests
- `ZOSServiceOperation::Invoke` with callback
	- Callback can be used to find the expected response-type based on usage of the following functions:
		- `OSuite::ZWebServiceClient::RetrieveRequest<OSuite::ZOFeed>`
		- `OSuite::ZWebServiceClient::RetrieveRequest<OSuite::ZOEntry>`
		- `OSuite::ZWebServiceClient::RetrieveRequest<OSuite::ZServiceOperationResult>`
	- See ReturnType below to find out what to set ReturnType to.
- `ZOSQueryManager::Push` => `ZOSServiceOperation::Execute` => `OSuite::ZWebServiceClient::ExecuteQuery` 

`OSuite::ZWebServiceClient::ExecuteQuery` will the try to get API endpoint. It bases this on the QueryMode, which can either be:
- QM_NONE //0
- QM_ENTITYSET //1
- QM_FUNCTIONIMPORT //2 

## ExecuteQuery with QM_NONE
This will always generate an internal 404, which causes the webservice to disconnect and show the Disconnected-dialog in-game (this happens with the `ShowDialog` in `ZOnlineManager::Update`).

## ExecuteQuery with QM_FUNCTIONIMPORT
`OSuite::ZOMetadata::FunctionImport` will be called and if this fails an internal 404 is generated.

It will then check if the FunctionImport has all the querystring-parameters specified. If something is missing, again an internal 404 is generated.

Otherwise, it will continue and call the endpoint based on the data from the FunctionImport. It can make either a GET or a POST request (`HttpMethod`). Based on the `ReturnType` of the FunctionImport it will decide to either expect a `ZOEntry`, `ZOServiceOperationResult` or a `ZOFeed`.

## ExecuteQuery with QM_ENTITYSET
`OSuite::ZOMetadata::EntitySet` will be called and always result in a GET-request for a `ZOFeed`.

# ReturnType
These are the different ReturnType:
- SVOP_FEED = 0x0 => `OSuite::ZOFeed`
- SVOP_ENTRY = 0x1 => `OSuite::ZOEntry`
- SVOP_VALUE = 0x2 => `OSuite::ZOServiceOperationResult`
- SVOP_VALUECOLLECTION = 0x3
- SVOP_VOID = 0x4

The following pseudocode shows how the game will convert the value of the `ReturnType` on a `EdmFunctionImport` to a ReturnType enumeration value:
```
this->entityName = ReturnType;
this->returnType = SVOP_VOID;

var isEntityType = !this->entityName.Contains("Edm.")

if(this->entityName->StartsWith("Collection"))
{
	if(isEntityType) {
		this->returnType = SVOP_FEED
	}
	else {
		this->returnType = SVOP_VALUECOLLECTION
	}

	this->entityName = "Collection(this->entityName)"
}
else if(isEntityType) {
	this->returnType = SVOP_ENTRY
}
else {
	this->returnType = SVOP_VALUE
}
```

# Json parsing
JSONTYPE_STRING = 0x0,
JSONTYPE_OBJECT = 0x1,
JSONTYPE_ARRAY = 0x2,

When `OSuite::ZAtomBase::ParseJson` is called:
- It will loop over all key-value pairs of the JSON-object that is passed in as the second argument.
- Each key-value pair will be passed to a `ParseJsonValue`-function, which is determined by the type of `ZAtomBase`-object passed in as the first argument.

There are a few of the `ZAtomBase`-objects in the game:
- `ZServiceOperationValue`
- `ZServiceOperationResult`
- `ZAtomFeed`
- `ZAtomEntry`
- `ZOMetadata`

The `ZOMetadata`-object is responsible for parsing the metadata-response from the API. It will call the `OSuite::ZOMetadata::ParseSchema`, which will call the `ParseJson`-function, which will call the `ParseJsonValue`-function of passed-in `ZEdmBase`-object.

There are a few of the `ZEdmBase`-objects in the game:
- `ZOEdmEntityType`
- `ZOEdmComplexType`
- `ZOEdmAssociation`
- `ZOEdmFunctionImport`
- `ZOEdmClientConfiguration`
	- NOTE: Instanced from `ZOMetadata->ParseJsonValue` 

## ZOEntry
- ZOEntry is always an EdmEntityType
- Can contain a "results" which then contains the ZOEntry, but this is not required.

## ZOFeed
- ZOFeed appears to support object and literal value. Not sure about array.
- object should contain a "results" and "__count", results is then treated as an array of ZOEntry.

## ZOServiceOperationResult
- ZOServiceOperationResult can be a single value or an array of values
- All values will be converted to a ZServiceOperationValue
- In-memory a ZOServiceOperationResult is always a list of 1 or more ZServiceOperationValue
- ZServiceOperationValue can be a single value or an object

# Scratchpad
These are just some random notes.
- `OSuite::ZOEntry::ParseJsonValue`, `OSuite::ZOFeed::ParseJsonValue` and `OSuite::ZOServiceOperationResult::ParseJsonValue` describe how to parse their respective type.
- The use of `OSuite::ZOEntry::Property` describes an expected property on a `ZOEntry`.
- The second argument of `OSuite::ZOQuery::EntitySet` is the name of the expected EntitySet,
- The first argument of `ZOSServiceOperation::Invoke` is the name of the expected `EdmImportFunction`