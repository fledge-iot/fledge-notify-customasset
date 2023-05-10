/*
 * Fledge "customasset" notification delivery plugin.
 *
 * Copyright (c) 2021 ACDP
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Sebastian Kropatschek, Thorsten Steuer
 */

/***********************************************************************
 * DISCLAIMER:
 *
 * All sample code is provided by ACDP for illustrative purposes only.
 * These examples have not been thoroughly tested under all conditions.
 * ACDP provides no guarantee nor implies any reliability,
 * serviceability, or function of these programs.
 * ALL PROGRAMS CONTAINED HEREIN ARE PROVIDED TO YOU "AS IS"
 * WITHOUT ANY WARRANTIES OF ANY KIND. ALL WARRANTIES INCLUDING
 * THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
 ***********************************************************************/

#include <string>
#include <plugin_api.h>
#include <config_category.h>
#include <logger.h>
#include <customasset.h>
#include <version.h>


#define PLUGIN_NAME "customasset"

const char * default_config = QUOTE({
        "plugin" : {
            "description" : "CustomAsset notification plugin",
            "type" : "string",
            "default" : PLUGIN_NAME,
            "readonly" : "true"
        },
        "customasset" : {
            "description" : "The customasset name to create for the notification",
            "type" : "string",
            "default" : "event",
            "order" : "1",
            "displayName" : "CustomAsset",
            "mandatory": "true"
        },
        "description" : {
            "description" : "The event description to add",
            "type" : "string",
            "default" : "Notification alert",
            "order" : "2",
            "displayName" : "Description"
        },
        "jsonconfig" : {
            "description" : "Add assets and datapoint the dilvery sevice needs to consider",
            "type" : "JSON",
            "default" : "{ \"asset_name\" : [{\"datapoint_name\": \"alias_name\"}],\"sinusoid\" : [{\"sinusoid\": \"cosinusoid\"}]}",
            "displayName" : "JSON Configuration",
            "order" : "3"
        },
        "enable" : {
            "description" : "A switch that can be used to enable or disable delivery of the customasset notification plugin.",
            "type" : "boolean",
            "displayName" : "Enabled",
            "default" : "false",
            "order" : "7"
        },
        "enableAuth" : {
            "description" : "A switch that can be used to enable or disable authentication.",
            "type" : "boolean",
            "displayName" : "Enable authentication",
            "default" : "false",
            "order" : "4"
        },
        "username" : {
            "description" : "User name for fledge instance",
            "type" : "string",
            "displayName" : "Username",
            "order" : "5",
            "default" : "max.mustermann",
            "validity": "enableAuth == \"true\""
        },
        "password" : {
            "description" : "Password for fledge instance",
            "type" : "password",
            "displayName" : "Password",
            "order" : "6",
            "default" : "pass",
            "validity" : "enableAuth == \"true\""
        }
});


using namespace std;
using namespace rapidjson;

/**
 * The Notification plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
        PLUGIN_NAME,              // Name
        VERSION,                  // Version
        SP_INGEST,                // Flags
        PLUGIN_TYPE_NOTIFICATION_DELIVERY,       // Type
        "1.0.0",                  // Interface version
        default_config	          // Default plugin configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}


/**
 * Initialise the plugin, called to get the plugin handle and setup the
 * plugin configuration
 *
 * @param config	The configuration category for the plugin
 * @return		An opaque handle that is used in all subsequent calls to the plugin
 */
PLUGIN_HANDLE plugin_init(ConfigCategory* config)
{
	CustomAsset *customasset = new CustomAsset(config);

	return (PLUGIN_HANDLE)customasset;
}

/**
 * Deliver received notification data
 *
 * @param handle		The plugin handle returned from plugin_init
 * @param deliveryName		The delivery category name
 * @param notificationName	The notification name
 * @param triggerReason		The trigger reason for notification
 * @param message		The message from notification
 */
bool plugin_deliver(PLUGIN_HANDLE handle,
                    const std::string& deliveryName,
                    const std::string& notificationName,
                    const std::string& triggerReason,
                    const std::string& message)
{
	Logger::getLogger()->info("CustomAsset notification plugin_deliver(): deliveryName=%s, notificationName=%s, triggerReason=%s, message=%s",
							deliveryName.c_str(), notificationName.c_str(), triggerReason.c_str(), message.c_str());
	CustomAsset *customasset = (CustomAsset *)handle;
	return customasset->notify(notificationName, triggerReason, message);
}

/**
 * Register a callback function used to ingest an customasset to the Fledge buffer
 */
void plugin_registerIngest(PLUGIN_HANDLE *handle, void *func, void *data)
{
	Logger::getLogger()->info("CustomAsset notification plugin: plugin_registerIngrest()");
	CustomAsset *customasset = (CustomAsset *)handle;

	customasset->registerIngest((FuncPtr)func, data);
	return;
}

/**
 * Reconfigure the plugin
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, string& newConfig)
{
	Logger::getLogger()->info("CustomAsset notification plugin: plugin_reconfigure()");
	CustomAsset *customasset = (CustomAsset *)handle;

	customasset->reconfigure(newConfig);
	return;
}

/**
 * Call the shutdown method in the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle)
{
	CustomAsset *customasset = (CustomAsset *)handle;
	delete customasset;
}

// End of extern "C"
};
