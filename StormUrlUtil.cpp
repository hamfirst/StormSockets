
#include "StormUrlUtil.h"

#include <string.h>

namespace StormSockets
{
  bool ValidSchemeCharacter(char c)
  {
    if (c >= 'a' && c <= 'z')
    {
      return true;
    }

    if (c >= 'A' && c <= 'Z')
    {
      return true;
    }

    if (c >= '0' && c <= '9')
    {
      return true;
    }

    if (c == '+' || c == '.' || c == '-')
    {
      return true;
    }

    return false;
  }

  bool ValidHostCharacter(char c)
  {
    if (c >= 'a' && c <= 'z')
    {
      return true;
    }

    if (c >= 'A' && c <= 'Z')
    {
      return true;
    }

    if (c >= '0' && c <= '9')
    {
      return true;
    }

    if (c == '.' || c == '-')
    {
      return true;
    }

    return false;
  }


  bool ParseURI(const char * uri, StormURI & out_uri)
  {
    auto colon = strstr(uri, "://");
    if (colon != nullptr)
    {
      if (*uri == ':')
      {
        return false;
      }

      while (*uri != ':')
      {
        if (ValidSchemeCharacter(*uri) == false)
        {
          return false;
        }

        out_uri.m_Protocol.push_back(*uri);
        uri++;
      }

      uri++;
      if (*uri != '/')
      {
        return false;
      }

      uri++;
      if (*uri != '/')
      {
        return false;
      }

      uri++;
    }
    else
    {
      if (*uri == '/')
      {
        uri++;
        if (*uri != '/')
        {
          return false;
        }

        uri++;
      }

      out_uri.m_Protocol = "http";
    }

    if (*uri == '/' || *uri == ':')
    {
      return false;
    }

    while (*uri != '/' && *uri != ':')
    {
      if (*uri == 0)
      {
        out_uri.m_Uri = "/";
        return true;
      }

      if (ValidHostCharacter(*uri) == false)
      {
        return false;
      }

      out_uri.m_Host.push_back(*uri);
      uri++;
    }

    if (*uri == ':')
    {
      uri++;
      if (*uri == '/')
      {
        return false;
      }

      while (*uri != '/')
      {
        if (*uri == 0)
        {
          out_uri.m_Uri = "/";
          return true;
        }

        if (*uri < '0' || *uri > '9')
        {
          return false;
        }

        out_uri.m_Port.push_back(*uri);
        uri++;
      }
    }

    out_uri.m_Uri.push_back(*uri);
    uri++;

    while (*uri != 0 && *uri != '#')
    {
      out_uri.m_Uri.push_back(*uri);
      uri++;
    }

    return true;
  }

  std::string EncodeURL(const char * url)
  {
    std::string out;
    while (*url != 0)
    {
      char c = *url;

      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') 
      {
        out.push_back(c);
      }
      else
      {
        out.push_back('%');
        
        char val = (c >> 4) & 0xF;
        if (val < 10)
        {
          out.push_back(val + '0');
        }
        else
        {
          out.push_back(val + 'A' - 10);
        }

        val = (c) & 0xF;
        if (val < 10)
        {
          out.push_back(val + '0');
        }
        else
        {
          out.push_back(val + 'A' - 10);
        }
      }

      url++;
    }

    return out;
  }

  bool DecodeURL(const char * url, std::string & out)
  {
    while (*url != 0)
    {
      if (*url == '%')
      {
        url++;
        char c = *url;

        char val = 0;
        if (c >= '0' && c <= '9')
        {
          val += c - '0';
        }
        else if (c >= 'A' && c <= 'F')
        {
          val += c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f')
        {
          val += c - 'a' + 10;
        }
        else
        {
          return false;
        }

        val <<= 4;

        url++;
        c = *url;
        if (c >= '0' && c <= '9')
        {
          val += c - '0';
        }
        else if (c >= 'A' && c <= 'F')
        {
          val += c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f')
        {
          val += c - 'a' + 10;
        }
        else
        {
          return false;
        }

        out.push_back(val);
      }
      else
      {
        out.push_back(*url);
      }

      url++;
    }

    return true;
  }

  template <typename MapType>
  std::string EncodeURLArgsMap(const MapType & args)
  {
    std::string out;
    auto itr = args.begin();
    if (itr != args.end())
    {
      while (true)
      {
        auto & kv = *itr;
        out += EncodeURL(kv.first.data());
        out += '=';
        out += EncodeURL(kv.second.data());

        ++itr;

        if (itr == args.end())
        {
          break;
        }
        else
        {
          out += '&';
        }
      }
    }

    return out;
  }

  std::string EncodeURLArgs(const std::multimap<std::string, std::string> & args)
  {
    return EncodeURLArgsMap(args);
  }

  std::string EncodeURLArgs(const std::map<std::string, std::string> & args)
  {
    return EncodeURLArgsMap(args);
  }

  template <typename MapType>
  std::string EncodeURLRequestMap(const std::string & uri_path, const MapType & args)
  {
    std::string out = uri_path;
    if (args.size() > 0)
    {
      out.push_back('?');

      auto itr = args.begin();      
      if(itr != args.end())
      {
        while (true)
        {
          auto & kv = *itr;
          out += EncodeURL(kv.first.data());
          out += '=';
          out += EncodeURL(kv.second.data());

          ++itr;

          if (itr == args.end())
          {
            break;
          }
          else
          {
            out += '&';
          }
        }
      }
    }

    return out;
  }

  std::string EncodeURLRequest(const std::string & uri_path, const std::multimap<std::string, std::string> & args)
  {
    return EncodeURLRequestMap(uri_path, args);
  }

  std::string EncodeURLRequest(const std::string & uri_path, const std::map<std::string, std::string> & args)
  {
    return EncodeURLRequestMap(uri_path, args);
  }

  template <typename MapType>
  bool DecodeUrlRequestMap(const char * uri, std::string & uri_path, MapType & args)
  {
    if (uri[0] != '/')
    {
      return false;
    }

    uri_path.push_back(*uri);
    uri++;

    while (*uri != '?' && *uri != 0)
    {
      uri_path.push_back(*uri);
      uri++;
    }

    if (*uri == '?')
    {
      do
      {
        uri++;

        std::string key;
        while (*uri != '=')
        {
          if (*uri == '&' || *uri == 0)
          {
            return false;
          }

          key.push_back(*uri);
          uri++;
        }

        std::string val;

        uri++;
        while (*uri != '&' && *uri != 0)
        {
          val.push_back(*uri);
          uri++;
        }

        std::string key_decoded;
        std::string val_decoded;

        if (DecodeURL(key.data(), key_decoded) == false || DecodeURL(val.data(), val_decoded) == false)
        {
          return false;
        }

        args.emplace(std::make_pair(key_decoded, val_decoded));
      } while (*uri == '&');
    }

    return true;
  }

  bool DecodeURLRequest(const char * uri, std::string & uri_path, std::multimap<std::string, std::string> & args)
  {
    return DecodeUrlRequestMap(uri, uri_path, args);
  }


  bool DecodeURLRequest(const char * uri, std::string & uri_path, std::map<std::string, std::string> & args)
  {
    return DecodeUrlRequestMap(uri, uri_path, args);
  }
}

