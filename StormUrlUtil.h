#pragma once

#include <string>
#include <map>

namespace StormSockets
{
  struct StormURI
  {
    std::string m_Protocol;
    std::string m_Host;
    std::string m_Port;
    std::string m_Uri;
  };

  bool ParseURI(const char * uri, StormURI & out_uri);

  std::string EncodeURL(const char * url);  
  bool DecodeURL(const char * url, std::string & out);

  std::string EncodeURLArgs(const std::multimap<std::string, std::string> & args);
  std::string EncodeURLArgs(const std::map<std::string, std::string> & args);

  std::string EncodeURLRequest(const std::string & uri_path, const std::multimap<std::string, std::string> & args);
  std::string EncodeURLRequest(const std::string & uri_path, const std::map<std::string, std::string> & args);


  bool DecodeURLRequest(const char * uri, std::string & uri_path, std::multimap<std::string, std::string> & args);
  bool DecodeURLRequest(const char * uri, std::string & uri_path, std::map<std::string, std::string> & args);

}