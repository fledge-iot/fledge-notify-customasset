/*
 * Fledge "customasset" notification delivery plugin.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include "customasset.h"
#include <logger.h>
#include <reading.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace std;
using namespace rapidjson;

const char * sample_store = QUOTE({
	"sinusoid" : {
		"sinusoid" : 0.5,
		"timestamp" : "2019-01-01 10:00:00.123456+00:00"
	},
	"opcuajob" : {
		"job" : "P60032085",
		"timestamp" : "2019-01-01 10:00:00.123456+00:00"
	},
	"opcuaproductid" : {
		"productid" : "S6866",
		"timestamp" : "2019-01-01 10:00:00.123456+00:00"
	}
});


/**
 * Construct a customasset notification plugin
 *
 * @param category	The configuration of the plugin
 */
CustomAsset::CustomAsset(ConfigCategory *category)
{
	m_customasset = category->getValue("customasset");
	m_description = category->getValue("description");
	m_json_config = category->getValue("jsonconfig");
	// TODO We have no access to the storage layer
	m_ingest = NULL;

	// FIXME: add error handling
	try{
			m_client = new HttpClient("localhost:8081");
			auto res = m_client->request("GET", "/fledge/audit?limit=1");
	} catch (exception& ex) {
			Logger::getLogger()->error("Failed to connect to server: %s", ex.what());
			throw;
	}
}

/**
 * The destructure for the customasset plugin
 */
CustomAsset::~CustomAsset()
{
	delete m_client;
	m_client = NULL;
}

/**
 * Send a notification via the CustomAsset by ingesting into the Fledge storage layer
 *
 * @param notificationName 	The name of this notification
 * @param triggerReason		Why the notification is being sent
 * @param message		The message to send
 */
void CustomAsset::notify(const string& notificationName, const string& triggerReason, const string& message)
{
	vector<Datapoint *>	datapoints;

	if (!m_ingest)
	{
		return;
	}

	DatapointValue dpv1(m_description);
	datapoints.push_back(new Datapoint("description", dpv1));

	Document doc;
	doc.Parse(triggerReason.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("reason"))
		{
			if (doc["reason"].IsString())
			{
				DatapointValue dpv3(doc["reason"].GetString());
				datapoints.push_back(new Datapoint("event", dpv3));
			}
			else if (doc["reason"].IsInt64())
			{
				DatapointValue dpv3((long) doc["reason"].GetInt64());
				datapoints.push_back(new Datapoint("event", dpv3));
			}
			else
			{
				Logger::getLogger()->error("The reason returned from the rule for delivery is of a bad type");
			}
		}
	}
	DatapointValue dpv4(notificationName);
	datapoints.push_back(new Datapoint("rule", dpv4));

	struct timeval now;
	gettimeofday(&now, NULL);

	#define DATE_TIME_BUFFER_LEN 52
	char date_time[DATE_TIME_BUFFER_LEN];
	char microsec[10];

	struct tm utcnow;
	gmtime_r(&now.tv_sec, &utcnow);

	std::strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &utcnow);
	snprintf(microsec, sizeof(microsec), ".%06lu", now.tv_usec);

	ostringstream ss;
	ss << date_time << microsec << "+00:00";


	//string utcnow_str;
	//utcnow_str = ss.str();

	// Should be probably taken to constructor to only get config once and save them in vectors or something
	Logger::getLogger()->warn("Config JSON: %s", m_json_config.c_str());
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());
	Logger::getLogger()->warn("Get Array");
	const rapidjson::Value& sub = configDoc["subscriptions"];

	Logger::getLogger()->warn("Start for ");
	for (rapidjson::Value::ConstValueIterator itr = sub.Begin(); itr != sub.End(); ++itr)
    {
			Logger::getLogger()->warn("Get Object of array ");
	     const rapidjson::Value& item = *itr;
	       for (rapidjson::Value::ConstMemberIterator itr2 = item.MemberBegin(); itr2 != item.MemberEnd(); ++itr2)
	       {
					 	Logger::getLogger()->warn("Key: %s",itr2->name.GetString());
					 	if(itr2->value.IsString()){
							Logger::getLogger()->warn("Key: %s Value: %s",itr2->name.GetString(), itr2->value.GetString());
						}else{
							const rapidjson::Value& datapoints = item[itr2->name.GetString()];
							for (SizeType i = 0; i < datapoints.Size(); i++){
								Logger::getLogger()->warn("Key: datapoints Value: %s",datapoints[i].GetString());
							}
						}
				 }
 	 }
	//---------------------------------------------------------------------------------------------

	m_store = getAssetReading();

	Logger::getLogger()->warn("Done getAssetReading");

	//m_store = ss.str();

	DatapointValue dpv5(m_store);
	datapoints.push_back(new Datapoint("store", dpv5));

	Reading customasset(m_customasset, datapoints);

	Logger::getLogger()->info("CustomAsset notification: %s", customasset.toJSON().c_str());

	(*m_ingest)(m_data, &customasset);
}

/**
 * Reconfigure the customasset delivery plugin
 *
 * @param newConfig	The new configuration
 */
void CustomAsset::reconfigure(const string& newConfig)
{
	ConfigCategory category("new", newConfig);
	m_customasset = category.getValue("customasset");
	m_description = category.getValue("description");
}



const std::string CustomAsset::getAssetReading()
{
	try {
			char url[256];

			Logger::getLogger()->warn("Hello getAssetReading");

			snprintf(url, sizeof(url), "/fledge/asset/%s?limit=1", "opcuajob");

			Logger::getLogger()->warn("url: %s", url);

			auto res = m_client->request("GET", url);
			if (res->status_code.compare("200 OK") == 0)
			{
				Logger::getLogger()->warn("HTTP CODE 200");
				ostringstream resultPayload;
				resultPayload << res->content.rdbuf();
				std::string result = resultPayload.str();
				Logger::getLogger()->warn("result: %s", result.c_str());
				return result;
			}

			ostringstream resultPayload;
			resultPayload << res->content.rdbuf();
			handleUnexpectedResponse("Fetch readings", res->status_code, resultPayload.str());

	} catch (exception& ex) {
			Logger::getLogger()->error("Failed to fetch asset: %s", ex.what());
			throw;

	} catch (exception* ex) {
			Logger::getLogger()->error("Failed to fetch asset: %s", ex->what());
			delete ex;
			throw exception();
	}

}

/**
 * Standard logging method for all interactions
 *
 * @param operation	The operation beign undertaken
 * @param responseCode	The HTTP response code
 * @param payload	The payload in the response message
 */
void CustomAsset::handleUnexpectedResponse(const char *operation, const std::string& responseCode,  const std::string& payload)
{
	rapidjson::Document doc;

	doc.Parse(payload.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("message"))
		{
			Logger::getLogger()->info("%s completed with result %s", operation, responseCode.c_str());
			Logger::getLogger()->error("%s: %s", operation, doc["message"].GetString());
		}
	}
	else
	{
		Logger::getLogger()->error("%s completed with result %s", operation, responseCode.c_str());
	}
}
