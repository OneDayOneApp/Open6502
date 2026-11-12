#include "http.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace nes {

namespace {

std::once_flag g_curl_once;

void global_init() {
  std::call_once(g_curl_once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

std::size_t write_string(char* data, std::size_t size, std::size_t count, void* user) {
  auto* out = static_cast<std::string*>(user);
  out->append(data, size * count);
  return size * count;
}

std::size_t write_file(char* data, std::size_t size, std::size_t count, void* user) {
  auto* file = static_cast<std::FILE*>(user);
  return std::fwrite(data, size, count, file);
}

std::size_t capture_header(char* data, std::size_t size, std::size_t count, void* user) {
  auto* headers = static_cast<std::map<std::string, std::string>*>(user);
  std::string line(data, size * count);
  const std::size_t colon = line.find(':');
  if (colon != std::string::npos) {
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.pop_back();
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    (*headers)[key] = value;
  }
  return size * count;
}

struct ProgressState {
  std::uint64_t offset = 0;
  std::uint64_t received = 0;
  std::uint64_t total = 0;
  const ProgressCallback* callback = nullptr;
  const std::atomic<bool>* cancel = nullptr;
};

int transfer_info(void* user, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
  auto* state = static_cast<ProgressState*>(user);
  if (state->cancel && state->cancel->load()) return 1;
  state->received = state->offset + static_cast<std::uint64_t>(dlnow > 0 ? dlnow : 0);
  state->total = dltotal > 0 ? state->offset + static_cast<std::uint64_t>(dltotal) : 0;
  if (state->callback && *state->callback) (*state->callback)(state->received, state->total);
  return 0;
}

curl_slist* make_headers(const std::vector<std::string>& headers) {
  curl_slist* list = nullptr;
  for (const auto& header : headers) list = curl_slist_append(list, header.c_str());
  return list;
}

}  // namespace

HttpClient::HttpClient(std::string proxy, int timeout_seconds)
    : proxy_(std::move(proxy)), timeout_seconds_(timeout_seconds > 0 ? timeout_seconds : 30) {
  global_init();
}

HttpResponse HttpClient::get(const std::string& url, const std::vector<std::string>& headers) const {
  HttpResponse response;
  CURL* curl = curl_easy_init();
  if (!curl) {
    response.error = "curl_easy_init failed";
    return response;
  }

  curl_slist* list = make_headers(headers);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, std::min(10, timeout_seconds_));
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "nesearch/0.2 (+https://github.com/Open6502/Open6502)");
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, capture_header);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
  if (!proxy_.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, proxy_.c_str());

  const CURLcode code = curl_easy_perform(curl);
  if (code != CURLE_OK) {
    response.error = curl_easy_strerror(code);
  }
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
  curl_slist_free_all(list);
  curl_easy_cleanup(curl);
  return response;
}

DownloadResult HttpClient::download(const std::string& url,
                                    const std::filesystem::path& part_path,
                                    bool resume,
                                    const ProgressCallback& progress,
                                    const std::atomic<bool>& cancel) const {
  DownloadResult result;
  std::error_code ec;
  std::uint64_t offset = 0;
  if (resume && std::filesystem::exists(part_path, ec)) {
    offset = std::filesystem::file_size(part_path, ec);
    if (ec) offset = 0;
  }
  if (offset > 0 && offset > 512ULL * 1024 * 1024) {
    offset = 0;
    std::filesystem::remove(part_path, ec);
  }

  std::FILE* file = std::fopen(part_path.string().c_str(), offset > 0 ? "ab" : "wb");
  if (!file) {
    result.error = "cannot open " + part_path.string();
    return result;
  }

  CURL* curl = curl_easy_init();
  if (!curl) {
    std::fclose(file);
    result.error = "curl_easy_init failed";
    return result;
  }

  ProgressState state;
  state.offset = offset;
  state.callback = &progress;
  state.cancel = &cancel;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "nesearch/0.2 (+https://github.com/Open6502/Open6502)");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_info);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);
  if (offset > 0) curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(offset));
  if (!proxy_.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, proxy_.c_str());

  const CURLcode code = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  result.status = status;
  std::fflush(file);
  std::fclose(file);
  curl_easy_cleanup(curl);

  if (code != CURLE_OK) {
    result.error = (code == CURLE_ABORTED_BY_CALLBACK) ? "canceled" : curl_easy_strerror(code);
    return result;
  }
  if (status == 416) {
    result.error = "range not satisfiable";
    return result;
  }
  if (status < 200 || status >= 300) {
    result.error = "HTTP " + std::to_string(status);
    return result;
  }

  std::filesystem::file_size(part_path, ec);
  result.bytes = ec ? 0 : std::filesystem::file_size(part_path, ec);
  result.ok = true;
  if (progress) progress(result.bytes, result.bytes);
  return result;
}

}  // namespace nes
