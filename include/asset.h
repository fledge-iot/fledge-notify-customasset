#ifndef _CUSTOMASSET_H
#define _CUSTOMASSET_H
#include <config_category.h>
#include <string>
#include <logger.h>
#include <storage_client.h>

typedef void (*FuncPtr)(void *, void *);
/**
 * A simple customasset notification class that sends an customasset
 * via Fledge to the systems north of Fledge
 */
class CustomAsset {
	public:
		CustomAsset(ConfigCategory *config);
		~CustomAsset();
		void	notify(const std::string& notificationName, const std::string& triggerReason, const std::string& message);
		void	reconfigure(const std::string& newConfig);
		void	registerIngest(FuncPtr ingest, void *data)
		{
			m_ingest = ingest;
			m_data = data;
		};
	private:
		std::string	m_customasset;
		std::string	m_description;
		FuncPtr		m_ingest;
		void		*m_data;
};
#endif
