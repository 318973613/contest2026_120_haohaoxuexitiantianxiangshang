#define TEST_REAL_AGENT_CONFIG
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent_config.h"
#include "infra/http_proxy.h"
#include "infra/vela_tls.h"
#include "llm/llm_proxy.h"
#include "voice/mimo_asr.h"
#include "voice/voice_asr.h"
#include "mbedtls/base64.h"

static const voice_asr_ops_t *asr;
static cJSON *request;
static int response_status = 200;
static const char *response = "{\"choices\":[{\"message\":{\"content\":\"answer\"}}]}";

int claw_config_get(const char *key, char *out, size_t size) {
    (void)key; if (size) out[0] = 0; return -1;
}
int claw_config_set(const char *key, const char *value) {
    (void)key; (void)value; return 0;
}
bool http_proxy_is_enabled(void) { return false; }
proxy_conn_t *proxy_conn_open(const char *host, int port, int timeout) {
    (void)host; (void)port; (void)timeout; assert(false); return NULL;
}
int proxy_conn_write(proxy_conn_t *p, const char *text, int len) {
    (void)p; (void)text; (void)len; assert(false); return -1;
}
int proxy_conn_read(proxy_conn_t *p, char *text, int len, int timeout) {
    (void)p; (void)text; (void)len; (void)timeout; assert(false); return -1;
}
void proxy_conn_close(proxy_conn_t *p) { (void)p; assert(false); }
int voice_asr_register(const voice_asr_ops_t *ops) { asr = ops; return 0; }

static int collect(const char *body, char *out, size_t cap) {
    cJSON_Delete(request); request = cJSON_Parse(body); assert(request);
    assert(strlen(response) < cap);
    snprintf(out, cap, "%s", response);
    return response_status;
}
int vela_https_post_json(const char *host, const char *port, const char *path,
    const vela_header_t *headers, const char *body, char *out, size_t cap) {
    (void)host; assert(!strcmp(port, "443"));
    assert(!strcmp(path, "/v1/chat/completions")); assert(headers);
    return collect(body, out, cap);
}
int vela_http_post_json(const char *host, const char *port, const char *path,
    const vela_header_t *headers, const char *body, char *out, size_t cap) {
    (void)host; (void)port; (void)path; (void)headers; (void)body; (void)out; (void)cap;
    assert(false); return -1;
}
int vela_https_request(const char *host, const char *port, const char *method,
    const char *path, const vela_header_t *headers, const char *body, size_t len,
    char *out, size_t cap, size_t *out_len) {
    assert(llm_is_mimo_host(host) && !strcmp(port, "443"));
    assert(!strcmp(method, "POST") && !strcmp(path, AGENT_LLM_MIMO_PATH));
    assert(len == strlen(body) && !strcmp(headers[0].name, "api-key"));
    int result = collect(body, out, cap); *out_len = strlen(out); return result;
}
static const char *string(cJSON *object, const char *key) {
    const char *value = cJSON_GetStringValue(cJSON_GetObjectItem(object, key));
    assert(value); return value;
}
static void check_mimo_options(void) {
    assert(!strcmp(string(request, "model"), "mimo-v2.5"));
    cJSON *limit = cJSON_GetObjectItem(request, "max_completion_tokens");
    assert(cJSON_IsNumber(limit) && limit->valueint == 768);
    assert(!cJSON_HasObjectItem(request, "max_tokens"));
    assert(!strcmp(string(cJSON_GetObjectItem(request, "thinking"), "type"), "disabled"));
}
int main(void) {
    char answer[512];
    assert(!strcmp(AGENT_LLM_MIMO_MODEL, "mimo-v2.5"));
    assert(llm_set_all(AGENT_LLM_MIMO_HOST, AGENT_LLM_MIMO_PATH, "443",
                       "fixture-not-a-real-key", "mimo-v2.5") == 0);
    assert(llm_chat("short answer", "[{\"role\":\"user\",\"content\":\"hi\"}]",
                    answer, sizeof(answer)) == 0);
    check_mimo_options(); assert(!strcmp(answer, "answer"));
    cJSON *messages = cJSON_Parse("[{\"role\":\"user\",\"content\":\"timer\"}]");
    llm_response_t result;
    assert(llm_chat_tools("short answer", messages,
        "[{\"name\":\"cron_add\",\"input_schema\":{\"type\":\"object\"}}]", &result) == 0);
    check_mimo_options(); assert(!strcmp(result.text, "answer"));
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(request, "tools")) == 1);
    llm_response_free(&result); cJSON_Delete(messages);
    puts("PASS actual MiMo simple/tool requests disable thinking and cap output at 768 tokens");

    assert(llm_set_all("api.openai.com", NULL, NULL, NULL, "fixture-model") == 0);
    assert(llm_chat("s", "[]", answer, sizeof(answer)) == 0);
    assert(!cJSON_HasObjectItem(request, "thinking"));
    assert(cJSON_GetObjectItem(request, "max_completion_tokens")->valueint == AGENT_LLM_MAX_TOKENS_OPENAI);
    assert(llm_set_all("example.test", NULL, NULL, NULL, "fixture-model") == 0);
    assert(llm_chat("s", "[]", answer, sizeof(answer)) == 0);
    assert(!cJSON_HasObjectItem(request, "thinking"));
    assert(cJSON_GetObjectItem(request, "max_tokens")->valueint == AGENT_LLM_MAX_TOKENS);
    puts("PASS other providers retain their existing request parameters");

    assert(llm_set_all(AGENT_LLM_MIMO_HOST, NULL, NULL, NULL, "mimo-v2.5") == 0);
    assert(mimo_asr_register() == 0 && asr && !asr->stream_open);
    unsigned char pcm[80000] = {0}; pcm[0] = 42;
    response = "{\"choices\":[{\"message\":{\"content\":\"recognized text\"}}]}";
    assert(asr->recognize(pcm, sizeof(pcm), answer, sizeof(answer)) == 0);
    assert(!strcmp(answer, "recognized text"));
    assert(!strcmp(string(request, "model"), "mimo-v2.5-asr"));
    cJSON *user = cJSON_GetArrayItem(cJSON_GetObjectItem(request, "messages"), 0);
    cJSON *audio = cJSON_GetArrayItem(cJSON_GetObjectItem(user, "content"), 0);
    const char *url = string(cJSON_GetObjectItem(audio, "input_audio"), "data");
    assert(!strncmp(url, "data:audio/wav;base64,", 22));
    unsigned char wav[80044]; size_t decoded;
    assert(mbedtls_base64_decode(wav, sizeof(wav), &decoded,
        (const unsigned char *)url + 22, strlen(url + 22)) == 0);
    assert(decoded == sizeof(wav) && !memcmp(wav, "RIFF", 4));
    assert(wav[22] == 1 && wav[24] == 0x80 && wav[25] == 0x3e && wav[34] == 16);
    assert(!memcmp(wav + 44, pcm, sizeof(pcm)));
    puts("PASS ASR sends complete 16 kHz mono PCM in a valid WAV data URL");

    const char *silence[] = {"", " \\t\\r\\n"};
    char json[256];
    for (size_t i = 0; i < 2; i++) {
        snprintf(json, sizeof(json), "{\"choices\":[{\"message\":{\"content\":\"%s\"}}]}", silence[i]);
        response = json; strcpy(answer, "old text");
        assert(asr->recognize(pcm, sizeof(pcm), answer, sizeof(answer)) == 0 && !answer[0]);
    }
    const char *bad[] = {"{", "{}", "{\"choices\":[]}",
                        "{\"choices\":[{\"message\":{\"content\":null}}]}"};
    for (size_t i = 0; i < 4; i++) {
        response = bad[i]; strcpy(answer, "old text");
        assert(asr->recognize(pcm, sizeof(pcm), answer, sizeof(answer)) == -EPROTO && !answer[0]);
    }
    response_status = 503;
    assert(asr->recognize(pcm, sizeof(pcm), answer, sizeof(answer)) == -EIO && !answer[0]);
    cJSON_Delete(request);
    puts("PASS silent ASR is successful and empty; malformed/HTTP errors never reuse stale text");
    return 0;
}
