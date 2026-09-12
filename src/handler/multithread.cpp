#include <future>
#include <thread>

#include "handler/settings.h"
#include "utils/network.h"
#include "webget.h"
#include "multithread.h"
//#include "vfs.h"

//safety lock for multi-thread
std::mutex on_emoji, on_rename, on_stream, on_time;

RegexMatchConfigs safe_get_emojis()
{
    guarded_mutex guard(on_emoji);
    return global.emojis;
}

RegexMatchConfigs safe_get_renames()
{
    guarded_mutex guard(on_rename);
    return global.renames;
}

RegexMatchConfigs safe_get_streams()
{
    guarded_mutex guard(on_stream);
    return global.streamNodeRules;
}

RegexMatchConfigs safe_get_times()
{
    guarded_mutex guard(on_time);
    return global.timeNodeRules;
}

void safe_set_emojis(RegexMatchConfigs data)
{
    guarded_mutex guard(on_emoji);
    global.emojis.swap(data);
}

void safe_set_renames(RegexMatchConfigs data)
{
    guarded_mutex guard(on_rename);
    global.renames.swap(data);
}

void safe_set_streams(RegexMatchConfigs data)
{
    guarded_mutex guard(on_stream);
    global.streamNodeRules.swap(data);
}

void safe_set_times(RegexMatchConfigs data)
{
    guarded_mutex guard(on_time);
    global.timeNodeRules.swap(data);
}

//convert GitHub web links (blob/raw) to raw.githubusercontent.com direct links,
//since fetching the web page returns HTML which cannot be parsed as config/ruleset
static std::string normalizeGithubLink(const std::string &path)
{
    static const std::string prefixes[] = {"https://github.com/", "http://github.com/"};
    std::string rest;
    for(const std::string &prefix : prefixes)
    {
        if(path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0)
        {
            rest = path.substr(prefix.size());
            break;
        }
    }
    if(rest.empty())
        return path;
    //rest = user/repo/<blob|raw>/ref/path...
    size_t pos1 = rest.find('/');
    if(pos1 == std::string::npos)
        return path;
    size_t pos2 = rest.find('/', pos1 + 1);
    if(pos2 == std::string::npos)
        return path;
    size_t pos3 = rest.find('/', pos2 + 1);
    if(pos3 == std::string::npos)
        return path;
    std::string mid = rest.substr(pos2 + 1, pos3 - pos2 - 1);
    if(mid != "blob" && mid != "raw")
        return path;
    return "https://raw.githubusercontent.com/" + rest.substr(0, pos2) + "/" + rest.substr(pos3 + 1);
}

std::shared_future<std::string> fetchFileAsync(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local, bool async)
{
    std::shared_future<std::string> retVal;
    /*if(vfs::vfs_exist(path))
        retVal = std::async(std::launch::async, [path](){return vfs::vfs_get(path);});
    else */if(find_local && fileExist(path, true))
        retVal = std::async(std::launch::async, [path](){return fileGet(path, true);});
    else if(isLink(path))
    {
        std::string link = normalizeGithubLink(path);
        retVal = std::async(std::launch::async, [link, proxy, cache_ttl](){return webGet(link, proxy, cache_ttl);});
    }
    else
        return std::async(std::launch::async, [](){return std::string();});
    if(!async)
        retVal.wait();
    return retVal;
}

std::string fetchFile(const std::string &path, const std::string &proxy, int cache_ttl, bool find_local)
{
    return fetchFileAsync(path, proxy, cache_ttl, find_local, false).get();
}
