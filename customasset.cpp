/*
 * Fledge "customasset" notification delivery plugin.
 *
 * Copyright (c) 2021 ACDP
 *
 * Released under the Apache 2.0 Licence
 *
 * Author:Thorsten Steuer, Sebastian Kropatschek
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

#include "customasset.h"
#include <logger.h>
#include <reading.h>
#include <rapidjson/error/en.h>
#include <string_utils.h>
#include <sstream>
#include <iomanip>

using namespace std;

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

	//get credentails
	enableAuth = category->getValue("enableAuth");
	password = category->getValue("password");
	username = category->getValue("username");

	//Get assetNames
	assetNames = getAssetNamesConfig();

	try{
			m_client = new HttpClient("localhost:8081");
			auto res = m_client->request("GET", "/fledge/audit?limit=1");
	} catch (exception& ex) {
			Logger::getLogger()->error("Failed to connect to server: %s", ex.what());
			throw;
	}

	if(enableAuth=="true"){
		getAuthToken();
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
bool CustomAsset::notify(const string& notificationName, const string& triggerReason, const string& message)
{
	vector<Datapoint *>	datapoints;

	if (!m_ingest)
	{
		return false;
	}

	DatapointValue dpv1(m_description);
	datapoints.push_back(new Datapoint("description", dpv1));

	Document doc;
	doc.Parse(triggerReason.c_str());
	if (!doc.HasParseError())
	{
		if(doc.HasMember("asset"))
		{
			if (doc["asset"].IsString())
			{
				// Logger::getLogger()->debug("ISSTRING: Delivery PLUGIN reason: %s",doc["asset"].GetString());
				DatapointValue dpv3(doc["asset"].GetString());
				datapoints.push_back(new Datapoint("action", dpv3));
			}
			else if (doc["asset"].IsObject())
			{
				rapidjson::StringBuffer buffer;
				rapidjson::Writer<StringBuffer> writer(buffer);
				doc["asset"].Accept(writer);
				std::string jsonString = buffer.GetString();
				jsonString = escape_json(jsonString);
				// Logger::getLogger()->debug("ISOBJECT: Delivery PLUGIN reason: %s",jsonString.c_str());
				DatapointValue dpv3(jsonString);
				datapoints.push_back(new Datapoint("action", dpv3));
			}
		}
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
				return false;
			}
		}
	}
	DatapointValue dpv4(notificationName);
	datapoints.push_back(new Datapoint("rule", dpv4));

	string readings;
	rapidjson::Document tempDoc;
	std::string readingJson;
	for(std::size_t i = 0; i < assetNames.size(); ++i)
	{
		assetDatapoints = getAssetDatapointsConfig(assetNames[i]);
	  readings = getAssetReading(assetNames[i]);
		Logger::getLogger()->debug("ASSETNAME: %s READING: %s", assetNames[i].c_str(), readings.c_str());
		//If reading is empty fledge returns empty array string
		if(readings=="[]"){
			Logger::getLogger()->debug("Assets is not available");
			readingJson = createReadingForEmptyAssets(assetNames[i]);
			Logger::getLogger()->debug("reading %s", readingJson.c_str());
			if(this->json_string.empty() == true){
				createJsonReadingObject(readingJson, assetNames[i]);
			}else{
				appendJsonReadingObject(readingJson, assetNames[i]);
			}
		}else{
			tempDoc.Parse<0>(readings.c_str());
			if(tempDoc.IsArray())
			{
				if(tempDoc.Size()==1)
				{
					Logger::getLogger()->debug("NOTIFICATION");
					rapidjson::Value& arrayEntry = tempDoc[0];
					rapidjson::Value& reading = arrayEntry["reading"];
					deleteUnwantedDatapoints(reading, assetDatapoints);
					rapidjson::StringBuffer reading_buffer;
					rapidjson::Writer<StringBuffer> reading_writer(reading_buffer);
					reading.Accept(reading_writer);
					readingJson = reading_buffer.GetString();
					readingJson= generateJsonReadingItem(assetNames[i], readingJson, arrayEntry["timestamp"].GetString(),assetDatapoints);
					if(this->json_string.empty() == true){
						createJsonReadingObject(readingJson, assetNames[i]);
					}else{
						appendJsonReadingObject(readingJson, assetNames[i]);
					}
				}else{
					for(SizeType i = 0; i < tempDoc.Size(); i++)
					{
						//needs to be implemented if mor than one reading should be considerd
					}
				}
			}
		}
	}

	this->json_string += "}";
	//Logger::getLogger()->debug("FINAL JSON %s", json_string.c_str());

	m_store = escape_json(json_string);
	json_string = "";

	DatapointValue dpv5(m_store);
	datapoints.push_back(new Datapoint("store", dpv5));

	Reading customasset(m_customasset, datapoints);

	(*m_ingest)(m_data, &customasset);
	return true;
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
	m_json_config = category.getValue("jsonconfig");
	password = category.getValue("password");
	username = category.getValue("username");
	enableAuth = category.getValue("enableAuth");
	assetNames = getAssetNamesConfig();
	if(enableAuth=="true"){
		getAuthToken();
	}
}



const std::string CustomAsset::getAssetReading(const std::string& assetName)
{
	if(enableAuth=="true"){
		SimpleWeb::CaseInsensitiveMultimap header;
		header.emplace("authorization ", this->token);
		try {
				auto res = m_client->request("GET", "/fledge/asset/" + urlEncode(assetName) + "?limit=1","",header);
				if (res->status_code.compare("200 OK") == 0)
				{
					ostringstream resultPayload;
					resultPayload << res->content.rdbuf();
					std::string result = resultPayload.str();
					return result;
				}
				ostringstream resultPayload;
				resultPayload << res->content.rdbuf();
				handleUnexpectedResponse("Fetch readings", res->status_code, resultPayload.str());
		} catch (exception& ex) {
				Logger::getLogger()->error("Failed to fetch asset: %s", ex.what());
				throw;
		}
		return "[]";
	}else{
		try {
				auto res = m_client->request("GET", "/fledge/asset/" + urlEncode(assetName) + "?limit=1");
				if (res->status_code.compare("200 OK") == 0)
				{
					ostringstream resultPayload;
					resultPayload << res->content.rdbuf();
					std::string result = resultPayload.str();
					return result;
				}
				ostringstream resultPayload;
				resultPayload << res->content.rdbuf();
				handleUnexpectedResponse("Fetch readings", res->status_code, resultPayload.str());
		} catch (exception& ex) {
				Logger::getLogger()->error("Failed to fetch asset: %s", ex.what());
				throw;
		}
		return "[]";
	}

return "[]";
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

std::string CustomAsset::escape_json(const std::string &s)
{
	std::ostringstream o;
	for (auto c = s.cbegin(); c != s.cend(); c++)
	{
		switch (*c)
		{
			case '"': o << "\\\""; break;
			case '\\': o << "\\\\"; break;
			case '\b': o << "\\b"; break;
			case '\f': o << "\\f"; break;
			case '\n': o << "\\n"; break;
			case '\r': o << "\\r"; break;
			case '\t': o << "\\t"; break;
			default:
				if ('\x00' <= *c && *c <= '\x1f') {
					o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
				}
				else
				{
					o << *c;
				}
		}
	}
	return o.str();
}

const std::vector<std::string> CustomAsset::getAssetNamesConfig()
{
	std::vector<std::string> assetNames;
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());

	for (Value::ConstMemberIterator itr = 	configDoc.MemberBegin();itr != 	configDoc.MemberEnd(); ++itr)
  {
		//Logger::getLogger()->debug("GETASSETNAMESCONFIG|Asset name: %s", itr->name.GetString());
		assetNames.push_back(itr->name.GetString());
 	}
	return assetNames;
}

const std::vector<std::string> CustomAsset::getAssetDatapointsConfig(const std::string &assetName)
{
	std::vector<std::string> datapointNames;
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());
	if(configDoc.HasMember(assetName.c_str()))
	{
		//Logger::getLogger()->debug("GETASSETDATAPOINTSSCONFIG|Has asset: %s", assetName.c_str());
		if(configDoc[assetName.c_str()].IsArray()){
			for (rapidjson::Value::ConstValueIterator itr = configDoc[assetName.c_str()].Begin(); itr != configDoc[assetName.c_str()].End(); ++itr){
	    	const rapidjson::Value& datapoint = *itr;
				if(datapoint.IsObject()){
					for (rapidjson::Value::ConstMemberIterator itr2 = datapoint.MemberBegin(); itr2 != datapoint.MemberEnd(); ++itr2) {
		        //Logger::getLogger()->debug("GETASSETDATAPOINTSSCONFIG|Datapoint Name:%s Alias:%s", itr2->name.GetString(), itr2->value.GetString());
						datapointNames.push_back(itr2->name.GetString());
					}
				}else{
					Logger::getLogger()->warn("Json Config has wrong format please submit objects in datapoint array");
				}
			}
		}else{
			Logger::getLogger()->warn("Json Config has wrong format please submit a array of objects");
		}
	}else{
		return {};
	}
	return datapointNames;
}

void CustomAsset::deleteUnwantedDatapoints(Value &reading, const std::vector<std::string> assetDatapoints)
{
	for (Value::ConstMemberIterator itr = reading.MemberBegin(); itr != reading.MemberEnd();)
	{
		if (std::find(assetDatapoints.begin(), assetDatapoints.end(), itr->name.GetString()) != assetDatapoints.end())
		{
			itr++;
		}else
		{
			itr = reading.EraseMember(itr);
		}
	}
}

const std::string CustomAsset::generateJsonReadingItem(const std::string& assetName,std::string reading, const std::string& timestamp, const std::vector<std::string>& assetDatapoints){
	for(auto datapoint : assetDatapoints){
		const std::string alias_name = getAliasNameConfig(assetName, datapoint);
		Logger::getLogger()->debug("ALIAS_NAME %s", alias_name.c_str());
		replace(reading,datapoint,alias_name);
	}
	Logger::getLogger()->debug("READING: %s TIMESTAMP: %s", reading.c_str(), timestamp.c_str());
	//Remove from brackets from String
	reading.pop_back();
	std::string actionJsonItem = reading + "," + "\"timestamp\":\""+ timestamp + " +0000\"}";
	Logger::getLogger()->debug("READING: %s" , actionJsonItem.c_str());
	return actionJsonItem;
}

void CustomAsset::createJsonReadingObject(const std::string& actionJsonItem, const std::string& assetName){
	Logger::getLogger()->debug("Append Item %s", actionJsonItem.c_str());
	this->json_string += "{\""+ assetName +"\":";
	this->json_string += actionJsonItem;
}

void CustomAsset::appendJsonReadingObject(const std::string& actionJsonItem,const std::string& assetName)
{
	Logger::getLogger()->debug("Append Item %s", actionJsonItem.c_str());
	this->json_string += ",\"" + assetName +"\":";
	this->json_string += actionJsonItem;
}

std::string CustomAsset::createReadingForEmptyAssets(const std::string& assetName){
	std::string alias_name;
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());
	std::string reading;
	if(configDoc[assetName.c_str()].IsArray()){
		for (rapidjson::Value::ConstValueIterator itr = configDoc[assetName.c_str()].Begin(); itr != configDoc[assetName.c_str()].End(); ++itr){
		const rapidjson::Value& datapoint = *itr;
			if(datapoint.IsObject()){
				for (rapidjson::Value::ConstMemberIterator itr2 = datapoint.MemberBegin(); itr2 != datapoint.MemberEnd(); ++itr2) {
					if(itr2->value.IsString()){
						alias_name = itr2->value.GetString();
						if(alias_name.empty() == true){
							alias_name=itr2->name.GetString();
						}
					}else{
						Logger::getLogger()->warn("Submit a String as alias_name");
					}
					if(reading.empty()==true){
						reading += "{\""+ alias_name + "\":\"\"";
					}else{
						reading +=",\""+ alias_name + "\":\"\"";
					}
				}
			}else{
				Logger::getLogger()->warn("Json Config has wrong format please submit objects in datapoint array");
			}
		}
	}else{
		Logger::getLogger()->warn("Json Config has wrong format please submit a array of objects");
	}
	reading += ",\"timestamp\":\"\"}";
	return reading;
}

const std::string CustomAsset::getAliasNameConfig(const std::string& assetName, const std::string& assetDatapoints){
	std::string alias_name;
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());
	if(configDoc.HasMember(assetName.c_str())){
		//Logger::getLogger()->debug("GETALIASNAMECONFIG|Has asset: %s", assetName.c_str());
		if(configDoc[assetName.c_str()].IsArray()){
			for (rapidjson::Value::ConstValueIterator itr = configDoc[assetName.c_str()].Begin(); itr != configDoc[assetName.c_str()].End(); ++itr){
	    	const rapidjson::Value& datapoint = *itr;
				if(datapoint.IsObject()){
					for (rapidjson::Value::ConstMemberIterator itr2 = datapoint.MemberBegin(); itr2 != datapoint.MemberEnd(); ++itr2) {
							if (assetDatapoints==itr2->name.GetString()){
								if(itr2->value.IsString()){
									alias_name = itr2->value.GetString();
									if(alias_name.empty() == true){
										alias_name=itr2->name.GetString();
									}
								}
							}
						}
					}else{
						Logger::getLogger()->warn("Json Config has wrong format please submit objects in datapoint array");
					}
				}
			}else{
				Logger::getLogger()->warn("Json Config has wrong format please submit a array of objects");
			}
	}else{
		return "";
	}
	return alias_name;
}

void CustomAsset::getAuthToken()
{
	std::string json_string = "{\"username\": \"" + this->username + "\",\"password\": \"" + this->password + "\"}";
	auto res = m_client->request("POST", "/fledge/login", json_string);
	std::ostringstream payload;
	payload << res->content.rdbuf();
	rapidjson::Document jsonDoc;
	jsonDoc.Parse(payload.str().c_str());
	if(jsonDoc.HasMember("token")){
		this->token = jsonDoc["token"].GetString();
	}else{
		Logger::getLogger()->error("Authentication was unsuccesfull: %s", payload.str().c_str());
	}
}

bool CustomAsset::replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}
