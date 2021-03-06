static const char *RcsId = "$Header$";
//+=============================================================================
//
// file :         DataBaseUtils.cpp
//
// description :  Various utility for the DataBase.
//
// project :      TANGO Device Servers
//
// author(s) :    A.Gotz, P.Verdier, JL Pons
//
// Copyright (C) :      2004,2005,2006,2007,2008,2009,2010,2011,2012,2013
//						European Synchrotron Radiation Facility
//                      BP 220, Grenoble 38043
//                      FRANCE
//
// This file is part of Tango.
//
// Tango is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tango is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Tango.  If not, see <http://www.gnu.org/licenses/>.
//
// $Revision$
// $Date$
//
// $HeadURL:$
//
//=============================================================================

#if HAVE_CONFIG_H
#include <ac_config.h>
#endif

#include <tango.h>
#include <DataBase.h>
#include <DataBaseClass.h>

#include <errmsg.h>

#include <stdio.h>

#ifdef _TG_WINDOWS_
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

using namespace std;

namespace DataBase_ns {

//+----------------------------------------------------------------------------
//
// method : 		DataBase::replace_wildcard(char *wildcard_c_str)
//
// description : 	utility method to replace all occurrences of
//			wildcards (*) with SQL wildcards % and to escape
//			all occurrences	of '%' and '_' with '\'
//
// in :			string **wildcard_c_str - wildcard C string
//
// out :		void - nothing
//
//-----------------------------------------------------------------------------
std::string DataBase::replace_wildcard(const char *wildcard_c_str)
{
	std::string wildcard(wildcard_c_str);
	std::string::size_type index;

	DEBUG_STREAM << "DataBase::replace_wildcard() wildcard in " << wildcard << std::endl;
// escape %

	index = 0;
	while ((index = wildcard.find('%',index)) != std::string::npos)
	{
		wildcard.insert(index, 1, '\\');
		index = index+2;
	}

// escape _

	index = 0;
	while ((index = wildcard.find('_',index)) != std::string::npos)
	{
		wildcard.insert(index, 1, '\\');
		index = index+2;
	}


// escape "

	index = 0;
	while ((index = wildcard.find('"',index)) != std::string::npos)
	{
		wildcard.insert(index, 1, '\\');
		index = index+2;
	}


// escape '

	index = 0;
	while ((index = wildcard.find('\'',index)) != std::string::npos)
	{
		wildcard.insert(index, 1, '\\');
		index = index+2;
	}

// replace wildcard * with %

	while ((index = wildcard.find('*')) != std::string::npos)
	{
		wildcard.replace(index, 1, 1, '%');
	}

	DEBUG_STREAM << "DataBase::replace_wildcard() wildcard out " << wildcard << std::endl;
	return wildcard;
}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::escape_string(char *string_c_str)
//
// description : 	utility method to escape all occurrences
//			of ' and " with '\'
//
// in :			const char *string_c_str -  C string to be modified.
//
// out :		string - The result string .
//
//-----------------------------------------------------------------------------
std::string DataBase::escape_string(const char *string_c_str)
{
	std::string escaped_string(string_c_str);
	std::string::size_type index;

	DEBUG_STREAM << "DataBase::escape_string() string in : " << escaped_string << std::endl;

	//	escape bakckslash
	index = 0;
	while ((index = escaped_string.find('\\',index)) != std::string::npos)
	{
		//	Check if double backslash already treated by client
		std::string	s = escaped_string.substr(index+1);

			//	Check if another escape sequence treated by client
		if (s.find('\"')==0 || s.find('\'')==0)
			index++;
		else
		{
			escaped_string.insert(index, 1, '\\');
			index += 2;
		}
	}

	//	escape "
	index = 0;
	while ((index = escaped_string.find('"',index)) != std::string::npos)
	{
		if (index==0)	//	Cannot have '\' at -1 !
		{
			escaped_string.insert(index, 1, '\\');
			index += 2;
		}
		else
		{
			//	Check if double quotes already treated by client
			std::string	s = escaped_string.substr(index-1);
			if (s.find('\\')==0)
				index++;
			else
			{
				escaped_string.insert(index, 1, '\\');
				index += 2;
			}
		}
	}


	//	escape '
	index = 0;
	while ((index = escaped_string.find('\'',index)) != std::string::npos)
	{
		if (index==0)	//	Cannot have '\' at -1 !
		{
			escaped_string.insert(index, 1, '\\');
			index += 2;
		}
		else
		{
			//	Check if simple quotes already treated by client
			std::string	s = escaped_string.substr(index-1);
			if (s.find('\\')==0)
				index++;
			else
			{
				escaped_string.insert(index, 1, '\\');
				index += 2;
			}
		}
	}

	DEBUG_STREAM << "DataBase::escaped_string() wildcard out : " << escaped_string << std::endl;
	return escaped_string;
}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::device_name_to_dfm(std::string &device_name,
//			          char **domain, char **family, char **member)
//
// description : 	utility function to return domain, family and member
//			from device name. Assumes device name has (optional)
//			protocol and instance stripped off i.e. conforms
//			to domain/family/member
//
// in :			char *devname - device name
//
// out :		bool - true or false
//
//-----------------------------------------------------------------------------
bool DataBase::device_name_to_dfm(std::string &devname, char domain[], char family[], char member[])
{
	std::string::size_type index, index2;

	DEBUG_STREAM << "DataBase::device_name_to_dfm() device name in " << devname << std::endl;

	index = devname.find('/');
	index2 = devname.find('/', index+1);

	(devname.substr(0,index)).copy(domain,index);
	domain[index] = '\0';

	(devname.substr(index+1,index2-index)).copy(family,index2-index);
	family[index2-index-1] = '\0';

	(devname.substr(index2+1)).copy(member,devname.length()-index2);
	member[devname.length()-index2-1] = '\0';

	DEBUG_STREAM << "DataBase::device_name_to_dfm() domain/family/member out " << domain << "/" << family << "/" << member << std::endl;

	return true;
}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::check_device_name(std::string *device_name)
//
// description : 	utility function to check whether device name conforms
//			to the TANGO naming convention of
//
//			[tango!taco:][//instance/]domain/family/member
//
// in :			string *device_name - device name
//
// out :		bool - true or false
//
//-----------------------------------------------------------------------------
bool DataBase::check_device_name(std::string &device_name_str)
{
	std::string devname(device_name_str);
	std::string::size_type index, index2;

	DEBUG_STREAM << "DataBase::check_device_name(): device_name in " << devname << std::endl;

// check there are no special characters which could be interpreted
// as wildcards or which are otherwise excluded

	if (devname.find('*') != std::string::npos) return false;

// check protocol - "tango:" | "taco:"

	if (devname.substr(0,6) == "tango:")
	{
		devname.erase(0,6);
	}
	else
	{
		if (devname.substr(0,5) == "taco:")
		{
			devname.erase(0,5);
		}
	}

// check instance - "//instance/"

	if (devname.substr(0,2) == "//")
	{
		index = devname.find('/',(string::size_type)2);
		if (index == 0 || index == std::string::npos)
		{
			return false;

		}
		devname.erase(0,index+1);
	}

// check name conforms to "D/F/M"

	index = devname.find('/');
	index2 = devname.find('/',index+1);

	if (index == 0 || index == std::string::npos || index2 == std::string::npos ||
	    index2-index <= 0 || devname.length() - index2 <= 0)
	{
		return false;
	}

	device_name_str = devname;

	DEBUG_STREAM << "DataBase::check_device_name(): device_name out " << device_name_str << std::endl;

	return true;
}

//+------------------------------------------------------------------
/**
 *	method:	DataBase::init_timing_stats
 *
 *	description:	method to initialise the timing statistics variables
 *	Returns void
 *
 * @param	none
 * @return	void
 *
 */
//+------------------------------------------------------------------
void DataBase::init_timing_stats()
{
	TimingStatsStruct new_timing_stats = {0.0, 0.0, 0.0, 0.0, 0.0};

	timing_stats_mutex.lock();

	timing_stats_map["DbImportDevice"] = new TimingStatsStruct;
	timing_stats_map["DbExportDevice"] = new TimingStatsStruct;
	timing_stats_map["DbGetHostServerList"] = new TimingStatsStruct;
	timing_stats_map["DbGetHostList"] = new TimingStatsStruct;
	timing_stats_map["DbGetServerList"] = new TimingStatsStruct;
	timing_stats_map["DbGetDevicePropertyList"] = new TimingStatsStruct;
	timing_stats_map["DbGetClassPropertyList"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceMemberList"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceFamilyList"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceDomainList"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceProperty"] = new TimingStatsStruct;
	timing_stats_map["DbPutDeviceProperty"] = new TimingStatsStruct;
	timing_stats_map["DbDeleteDeviceProperty"] = new TimingStatsStruct;
	timing_stats_map["DbInfo"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceClassList"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceAttributeProperty"] = new TimingStatsStruct;
	timing_stats_map["DbPutDeviceAttributeProperty"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceAttributeProperty2"] = new TimingStatsStruct;
	timing_stats_map["DbPutDeviceAttributeProperty2"] = new TimingStatsStruct;
	timing_stats_map["DbUnExportServer"] = new TimingStatsStruct;
	timing_stats_map["DbGetDeviceExportedList"] = new TimingStatsStruct;
	timing_stats_map["DbExportEvent"] = new TimingStatsStruct;
	timing_stats_map["DbImportEvent"] = new TimingStatsStruct;
	timing_stats_map["DbGetDataForServerCache"] = new TimingStatsStruct;
	timing_stats_map["DbPutClassProperty"] = new TimingStatsStruct;
	timing_stats_map["DbMySqlSelect"] = new TimingStatsStruct;
	timing_stats_map["DbGetDevicePipeProperty"] = new TimingStatsStruct;
	timing_stats_map["DbPutDevicePipeProperty"] = new TimingStatsStruct;

	timing_stats_size = timing_stats_map.size();
	timing_stats_average = new double[timing_stats_size];
	timing_stats_minimum = new double[timing_stats_size];
	timing_stats_maximum = new double[timing_stats_size];
	timing_stats_calls = new double[timing_stats_size];
	timing_stats_index = new Tango::DevString[timing_stats_size];

	//	now loop over map and initialise remaining variables
	std::map<std::string,TimingStatsStruct*>::iterator iter;
	int i=0;

	for (iter = timing_stats_map.begin(); iter != timing_stats_map.end(); iter++)
	{
		*(iter->second) = new_timing_stats;
		timing_stats_index[i] = (char*)malloc(strlen(iter->first.c_str())+1);
		strcpy(timing_stats_index[i],iter->first.c_str());
		i++;
	}

	timing_stats_mutex.unlock();
}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::check_history_tables()
//
// description : 	Return history id
//
//-----------------------------------------------------------------------------
void DataBase::check_history_tables()
{
	TangoSys_MemStream	sql_query_stream;
	MYSQL_RES *result;

	INFO_STREAM << "DataBase::check_history_tables(): entering" << std::endl;

	sql_query_stream.str("");
	sql_query_stream << "SELECT count(*) FROM property_device_hist";
	DEBUG_STREAM << "DataBase::check_history_tables(): sql_query " << sql_query_stream.str() << std::endl;
	result = query(sql_query_stream.str(),"check_history_tables()");
	mysql_free_result(result);
}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::get_id()
//
// description : 	Return history id
//					In this method, we don't use the classical query()
//					method in order to be sure that the UPDATE and the following
//					mysql_insert_id() are done using the same MySQL connection
//
//-----------------------------------------------------------------------------

Tango::DevULong64 DataBase::get_id(const char *name,int con_nb)
{
	TangoSys_MemStream sql_query;

//
// If no MySQL connection passed to this method,
// get one
//

	bool need_release = false;

	if (con_nb == -1)
	{
		con_nb = get_connection();
		need_release = true;
	}

    sql_query.str("");
    sql_query << "UPDATE " << name << "_history_id SET id=LAST_INSERT_ID(id+1)";
	std::string tmp_str = sql_query.str();

	if (mysql_real_query(conn_pool[con_nb].db, tmp_str.c_str(),tmp_str.length()) != 0)
	{
		TangoSys_OMemStream o;

		WARN_STREAM << "DataBase::get_id() failed to query TANGO database:" << std::endl;
		WARN_STREAM << "  query = " << tmp_str << std::endl;
		WARN_STREAM << " (SQL error=" << mysql_error(conn_pool[con_nb].db) << ")" << std::endl;

		o << "Failed to query TANGO database (error=" << mysql_error(conn_pool[con_nb].db) << ")" << std::ends;

		if (need_release == true)
			release_connection(con_nb);

		Tango::Except::throw_exception((const char *)DB_SQLError,o.str(),(const char *)"DataBase::get_id()");
	}

	my_ulonglong val = mysql_insert_id(conn_pool[con_nb].db);
	if (val == 0)
	{
		TangoSys_OMemStream o;

		o << "Failed to get history id : " << name;

		if (need_release == true)
			release_connection(con_nb);

		Tango::Except::throw_exception((const char *)DB_SQLError,o.str(),(const char *)"DataBase::get_id()");
	}

	if (need_release == true)
		release_connection(con_nb);

	return (Tango::DevULong64)val;
}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::simple_query()
//
// description : 	Execute a SQL query , ignore the result.
//
//-----------------------------------------------------------------------------
void DataBase::simple_query(std::string sql_query,const char *method,int con_nb)
{

//
// If no MySQL connection passed to this method,
// get one
//

	bool need_release = false;

	if (con_nb == -1)
	{
		con_nb = get_connection();
		need_release = true;
	}

	DEBUG_STREAM << "Using MySQL connection with semaphore " << con_nb << std::endl;

//
// Call MySQL
//

	if (mysql_real_query(conn_pool[con_nb].db, sql_query.c_str(),sql_query.length()) != 0)
	{
		TangoSys_OMemStream o;
		TangoSys_OMemStream o2;

		WARN_STREAM << "DataBase::" << method << " failed to query TANGO database:" << std::endl;
		WARN_STREAM << "  query = " << sql_query << std::endl;
		WARN_STREAM << " (SQL error=" << mysql_error(conn_pool[con_nb].db) << ")" << std::endl;

		o << "Failed to query TANGO database (error=" << mysql_error(conn_pool[con_nb].db) << ")";
		o << "\n.The query was: " << sql_query << std::ends;
		o2 << "DataBase::" << method << std::ends;

		if (need_release == true)
			release_connection(con_nb);

		Tango::Except::throw_exception((const char *)DB_SQLError,o.str(),o2.str());
	}

	if (need_release)
		release_connection(con_nb);

}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::query()
//
// description : 	Execute a SQL query and return the result.
//
//-----------------------------------------------------------------------------
MYSQL_RES *DataBase::query(std::string sql_query,const char *method,int con_nb)
{
	MYSQL_RES *result;

//
// If no MySQL connection passed to this method,
// get one
//

	bool need_release = false;

	if (con_nb == -1)
	{
		con_nb = get_connection();
		need_release = true;
	}

	DEBUG_STREAM << "Using MySQL connection with semaphore " << con_nb << std::endl;

//
// Call MySQL
//

	if (mysql_real_query(conn_pool[con_nb].db, sql_query.c_str(),sql_query.length()) != 0)
	{
		TangoSys_OMemStream o;
		TangoSys_OMemStream o2;

		WARN_STREAM << "DataBase::" << method << " failed to query TANGO database:" << std::endl;
		WARN_STREAM << "  query = " << sql_query << std::endl;
		WARN_STREAM << " (SQL error=" << mysql_error(conn_pool[con_nb].db) << ")" << std::endl;

		o << "Failed to query TANGO database (error=" << mysql_error(conn_pool[con_nb].db) << ")";
		o << "\nThe query was: " << sql_query << std::ends;
		o2 << "DataBase::" << method << std::ends;

		if (need_release == true)
			release_connection(con_nb);

		Tango::Except::throw_exception((const char *)DB_SQLError,o.str(),o2.str());
	}

	if ((result = mysql_store_result(conn_pool[con_nb].db)) == NULL)
	{
		TangoSys_OMemStream o;
		TangoSys_OMemStream o2;

		WARN_STREAM << "DataBase:: " << method << " : mysql_store_result() failed  (error=" << mysql_error(conn_pool[con_nb].db) << ")" << std::endl;

		o << "mysql_store_result() failed (error=" << mysql_error(conn_pool[con_nb].db) << ")";
		o2 << "DataBase::" << method;

		if (need_release == true)
			release_connection(con_nb);

		Tango::Except::throw_exception((const char *)DB_SQLError,o.str(),o2.str());
	}

	if (need_release)
		release_connection(con_nb);

	return result;

}

//+----------------------------------------------------------------------------
//
// method : 		DataBase::get_connection()
//
// description : 	Get a MySQl connection from the connection pool
//
//-----------------------------------------------------------------------------
int DataBase::get_connection()
{

//
// Get a MySQL connection and lock it
// If none available, wait for one
//

	int loop = 0;
	while (conn_pool[loop].the_sema.trywait() == 0)
	{
		loop++;
		if (loop == conn_pool_size)
		{
			int sem_to_wait;
			{
				omni_mutex_lock oml(sem_wait_mutex);
				sem_to_wait = last_sem_wait++;
				if (last_sem_wait == conn_pool_size)
					last_sem_wait = 0;
			}
			loop = sem_to_wait;
			WARN_STREAM << "Waiting for one free MySQL connection on semaphore " << loop << std::endl;
			conn_pool[loop].the_sema.wait();
			break;
		}
	}

	return loop;
}


//+------------------------------------------------------------------
/**
 *	method:	purge_property()
 *
 *	description:	purge a property history table.
 *
 */
//+------------------------------------------------------------------
void DataBase::purge_property(const char *table,const char *field,const char *object,const char *name,int con_nb) {

  TangoSys_MemStream sql_query;
  MYSQL_RES *result;
  MYSQL_ROW row2;

  sql_query.str("");
  sql_query << "SELECT DISTINCT id,date FROM " << table
            << " WHERE " << field << "=\"" << object << "\" AND name=\"" << name
            << "\" ORDER by date";

  result = query(sql_query.str(),"purge_property()",con_nb);
  int nb_item = mysql_num_rows(result);

  if(nb_item>historyDepth) {
    // Purge
    int toDelete = nb_item-historyDepth;
    for(int j=0;j<toDelete;j++) {
        row2 = mysql_fetch_row(result);
        sql_query.str("");
        sql_query << "DELETE FROM " << table << " WHERE id='" << row2[0] << "'";
		simple_query(sql_query.str(),"purge_property()",con_nb);
    }
  }

  mysql_free_result(result);
}

//+------------------------------------------------------------------
/**
 *	method:	purge_attribute_property()
 *
 *	description:	purge an attribute property history table.
 *
 */
//+------------------------------------------------------------------
void DataBase::purge_att_property(const char *table,const char *field,const char *object,const char *attribute,const char *name,int con_nb) {

  TangoSys_MemStream sql_query;
  MYSQL_RES *result;
  MYSQL_ROW row2;

  //cout << "purge_att_property(" << object << "," << attribute << "," << name << ")" << std::endl;

  sql_query.str("");
  sql_query << "SELECT DISTINCT id,date FROM " << table
            << " WHERE " << field << "=\"" << object << "\" AND name=\"" << name
            << "\" AND attribute=\"" << attribute << "\" ORDER by date";

  result = query(sql_query.str(),"purge_att_property()",con_nb);
  int nb_item = mysql_num_rows(result);

  if(nb_item>historyDepth) {
    // Purge
    int toDelete = nb_item-historyDepth;
    for(int j=0;j<toDelete;j++) {
		row2 = mysql_fetch_row(result);
        sql_query.str("");
        sql_query << "DELETE FROM " << table << " WHERE id='" << row2[0] << "'";
		simple_query(sql_query.str(),"purge_att_property()",con_nb);
    }
  }
  mysql_free_result(result);
}

//+------------------------------------------------------------------
/**
 *	method:	purge_pipe_property()
 *
 *	description:	purge an pipe property history table.
 *
 */
//+------------------------------------------------------------------
void DataBase::purge_pipe_property(const char *table,const char *field,const char *object,const char *pipe,const char *name,int con_nb) {

  TangoSys_MemStream sql_query;
  MYSQL_RES *result;
  MYSQL_ROW row2;

  //cout << "purge_pipe_property(" << object << "," << pipe << "," << name << ")" << std::endl;

  sql_query.str("");
  sql_query << "SELECT DISTINCT id,date FROM " << table
            << " WHERE " << field << "=\"" << object << "\" AND name=\"" << name
            << "\" AND pipe=\"" << pipe << "\" ORDER by date";

  result = query(sql_query.str(),"purge_pipe_property()",con_nb);
  int nb_item = mysql_num_rows(result);

  if(nb_item>historyDepth) {
    // Purge
    int toDelete = nb_item-historyDepth;
    for(int j=0;j<toDelete;j++) {
		row2 = mysql_fetch_row(result);
        sql_query.str("");
        sql_query << "DELETE FROM " << table << " WHERE id='" << row2[0] << "'";
		simple_query(sql_query.str(),"purge_pipe_property()",con_nb);
    }
  }
  mysql_free_result(result);
}

//+------------------------------------------------------------------
/**
 *	method:	base_connect()
 *
 *	description:	Basic action to build a Mysql connection
 *
 */
//+------------------------------------------------------------------

void DataBase::base_connect(int loop)
{

//
// Initialise mysql database structure and connect to TANGO database
//

	conn_pool[loop].db = mysql_init(conn_pool[loop].db);
	mysql_options(conn_pool[loop].db,MYSQL_READ_DEFAULT_GROUP,"client");

#if   (MYSQL_VERSION_ID > 50000)

	//C. Scafuri: auto reconnection has been off since 5.0.3. From 5.0.13 it is possible to set it as an option
	// with reconnection enabled DataBase keeps working after timeouts and mysql shutdown/restart
	if(mysql_get_client_version() >= 50013)
	{
		my_bool my_auto_reconnect=1;
		if (mysql_options(conn_pool[loop].db,MYSQL_OPT_RECONNECT,&my_auto_reconnect) !=0)
		{
			ERROR_STREAM << "DataBase: error setting mysql auto reconnection: " << mysql_error(conn_pool[loop].db) << std::endl;
		}
		else
		{
			WARN_STREAM << "DataBase: set mysql auto reconnect to true" << std::endl;
		}
	}
#endif
}

//+------------------------------------------------------------------
/**
 *	method:	create_connection_pool()
 *
 *	description:	Create the MySQL connections pool
 *
 */
//+------------------------------------------------------------------

void DataBase::create_connection_pool(const char *mysql_user,
                                      const char *mysql_password,
                                      const char *mysql_host,
                                      const char *mysql_name)
{
#ifndef HAVE_CONFIG_H
	char *database = (char *)"tango";
#else
	char *database = (char *)TANGO_DB_NAME;
#endif

//
// Init MySQL db name (locally and as data member)
//

    if (mysql_name != NULL)
    {
        database = const_cast<char *>(mysql_name);
    }
    mysql_db_name = database;

//
// Check on provided MySQl user and password
//

	if (mysql_user != NULL && mysql_password != NULL)
	{
		WARN_STREAM << "DataBase::create_connection_pool(): mysql database user =  " << mysql_user
	           	 << " , password = " << mysql_password << std::endl;
	}

	const char *host;
	std::string my_host;
	std::string ho,port;
	unsigned int port_num = 0;

	if (mysql_host != NULL)
	{
		my_host = mysql_host;
		WARN_STREAM << "DataBase::create_connection_pool(): mysql host = " << mysql_host << std::endl;
		std::string::size_type pos = my_host.find(':');
		if (pos != std::string::npos)
		{
			ho = my_host.substr(0,pos);
			pos++;
			port = my_host.substr(pos);
			std::stringstream ss(port);
			ss >> port_num;
			if (!ss)
				port_num = 0;
			host = ho.c_str();
		}
		else
			host = my_host.c_str();
		WARN_STREAM << "DataBase::create_connection_pool(): mysql host = " << host << ", port = " << port_num << std::endl;
	}
	else
		host = NULL;

	for (int loop = 0;loop < conn_pool_size;loop++)
	{

		base_connect(loop);

//
// Inmplement a retry. On some OS (Ubuntu 10.10), it may happens that MySQl needs some time to start.
// This retry should cover this case
// We also have to support case when this server is started while mysql is not ready yet
// (this has been experienced on Ubuntu after a reboot when the ureadahead cache being invalidated
// by a package installing file in /etc/init.d
// Bloody problem!!!
//


		WARN_STREAM << "Going to connect to MySQL for conn. " << loop << std::endl;
		if (!mysql_real_connect(conn_pool[loop].db, host, mysql_user, mysql_password, database, port_num, NULL, CLIENT_MULTI_STATEMENTS | CLIENT_FOUND_ROWS))
		{
			if (loop == 0)
			{
				int retry = 5;
				while (retry > 0)
				{
					sleep(1);
					int db_err = mysql_errno(conn_pool[loop].db);
					WARN_STREAM << "Connection to MySQL failed with error " << db_err << std::endl;
					if (db_err == CR_CONNECTION_ERROR || db_err == CR_CONN_HOST_ERROR)
					{
						mysql_close(conn_pool[loop].db);
						conn_pool[loop].db = NULL;
						base_connect(loop);
					}
					WARN_STREAM << "Going to retry to connect to MySQL for connection " << loop << std::endl;
					if (!mysql_real_connect(conn_pool[loop].db, host, mysql_user, mysql_password, database, port_num, NULL, CLIENT_MULTI_STATEMENTS | CLIENT_FOUND_ROWS))
					{
						WARN_STREAM << "Connection to MySQL (re-try) failed with error " << mysql_errno(conn_pool[loop].db) << std::endl;
						retry--;
						if (retry == 0)
						{
							WARN_STREAM << "Throw exception because no MySQL connection possible after 5 re-tries" << std::endl;
							TangoSys_MemStream out_stream;
							out_stream << "Failed to connect to TANGO database (error = " << mysql_error(conn_pool[loop].db) << ")" << std::ends;

							Tango::Except::throw_exception((const char *)"CANNOT_CONNECT_MYSQL",
												out_stream.str(),
												(const char *)"DataBase::init_device()");
						}
					}
					else
					{
						WARN_STREAM << "MySQL connection succeed after retry" << std::endl;
						retry = 0;
					}
				}
			}
			else
			{
				WARN_STREAM << "Failed to connect to MySQL for conn. " << loop << ". No re-try in this case" << std::endl;
				TangoSys_MemStream out_stream;
				out_stream << "Failed to connect to TANGO database (error = " << mysql_error(conn_pool[loop].db) << ")" << std::ends;

				Tango::Except::throw_exception((const char *)"CANNOT_CONNECT_MYSQL",
												out_stream.str(),
												(const char *)"DataBase::init_device()");
			}
		}
	}

	mysql_svr_version = mysql_get_server_version(conn_pool[0].db);
	last_sem_wait = 0;

}

//+------------------------------------------------------------------
/**
 *	method:	host_port_from_ior()
 *
 *	description:	Get host and port from a device IOR
 *
 */
//+------------------------------------------------------------------

bool DataBase::host_port_from_ior(const char *iorstr,std::string &h_p)
{
	size_t s = (iorstr ? strlen(iorstr) : 0);

  	if (s < 4)
    	return false;

  	const char *p = iorstr;
  	if (p[0] != 'I' || p[1] != 'O' || p[2] != 'R' || p[3] != ':')
    	return false;

  	s = (s - 4) / 2;  // how many octets are there in the string
  	p += 4;

  	cdrMemoryStream buf((CORBA::ULong)s,0);

  	for (int i=0; i<(int)s; i++)
	{
    	int j = i*2;
    	CORBA::Octet v;

    	if (p[j] >= '0' && p[j] <= '9')
		{
      		v = ((p[j] - '0') << 4);
    	}
    	else if (p[j] >= 'a' && p[j] <= 'f')
		{
      		v = ((p[j] - 'a' + 10) << 4);
    	}
    	else if (p[j] >= 'A' && p[j] <= 'F')
		{
      		v = ((p[j] - 'A' + 10) << 4);
    	}
    	else
      		return false;

    	if (p[j+1] >= '0' && p[j+1] <= '9')
		{
      		v += (p[j+1] - '0');
    	}
    	else if (p[j+1] >= 'a' && p[j+1] <= 'f')
		{
      		v += (p[j+1] - 'a' + 10);
    	}
    	else if (p[j+1] >= 'A' && p[j+1] <= 'F')
		{
      		v += (p[j+1] - 'A' + 10);
    	}
    	else
      		return false;

    	buf.marshalOctet(v);
  	}

  	buf.rewindInputPtr();
  	CORBA::Boolean b = buf.unmarshalBoolean();
  	buf.setByteSwapFlag(b);

  	IOP::IOR ior;

  	ior.type_id = IOP::IOR::unmarshaltype_id(buf);
  	ior.profiles <<= buf;


    if (ior.profiles.length() == 0 && strlen(ior.type_id) == 0)
	{
      	return false;
    }
    else
	{
      	for (unsigned long count=0; count < ior.profiles.length(); count++)
		{
			if (ior.profiles[count].tag == IOP::TAG_INTERNET_IOP)
			{
	  			IIOP::ProfileBody pBody;
	  			IIOP::unmarshalProfile(ior.profiles[count],pBody);

//
// Three possible cases for host name:
// 1 - The host is stored in IOR as IP numbers
// 2 - The host name is stored in IOR as the canonical host name
// 3 - The FQDN is stored in IOR
// We allways try to get the host name as the FQDN
//

				ho = pBody.address.host.in();
				bool host_is_name = false;

				std::string::size_type pos = ho.find('.');
				if (pos == std::string::npos)
					host_is_name = true;
				else
				{
					for (unsigned int loop =0;loop < pos;++loop)
					{
						if (isdigit((int)ho[loop]) == 0)
						{
							host_is_name = true;
							break;
						}
					}
				}

				if (host_is_name == false)
				{
					struct sockaddr_in s;
					char service[20];

					s.sin_family = AF_INET;
					int res;
#ifdef _TG_WINDOWS_
					s.sin_addr.s_addr = inet_addr(ho.c_str());
					if (s.sin_addr.s_addr != INADDR_NONE)
#else
					res = inet_pton(AF_INET,ho.c_str(),&(s.sin_addr.s_addr));
					if (res == 1)
#endif
					{
						res = getnameinfo((const struct sockaddr *)&s,sizeof(s),ho_name,sizeof(ho_name),service,sizeof(service),0);
						if (res == 0)
						{
							h_p = ho_name;
							h_p = h_p + ':';
						}
						else
							h_p = ho + ':';
					}
					else
						h_p = ho + ':';
				}
				else
				{
					if (pos == std::string::npos)
					{
						Tango::DeviceProxy::get_fqdn(ho);
					}

					h_p = ho + ':';
				}

//
// Add port number
//

				std::stringstream ss;
				ss << pBody.address.port;
				h_p = h_p + ss.str();

				break;
			}
		}
	}

	return true;
}


//+------------------------------------------------------------------
/**
 *	method:	AutoLock class ctor and dtor
 *
 *	description:	AutoLock is a small helper class which get a
 *					MySQL connection from the pool and which lock
 *					table(s). The exact lock statemen is passed to the
 *					ctor as a parameter
 *					The dtor release the table(s) lock
 *
 */
//+------------------------------------------------------------------

AutoLock::AutoLock(const char *lock_cmd,DataBase *db):the_db(db)
{
	con_nb = the_db->get_connection();
	TangoSys_MemStream sql_query_stream;

	sql_query_stream << lock_cmd;
	try
	{
		the_db->simple_query(sql_query_stream.str(),"AutoLock",con_nb);
	}
	catch (...)
	{
		the_db->release_connection(con_nb);
		throw;
	}
}

AutoLock::~AutoLock()
{
	TangoSys_MemStream sql_query_stream;
	sql_query_stream << "UNLOCK TABLES";
	the_db->simple_query(sql_query_stream.str(),"~AutoLock",con_nb);
	the_db->release_connection(con_nb);
}

}
