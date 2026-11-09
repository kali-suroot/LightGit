/*
 * test_json.c — 轻量 JSON 解析器单元测试
 */
#include "test_framework.h"
#include "lightgit/common/json.h"

#include <string.h>

static int LgTestJsonBasic(void)
{
    const char *j = "{\"login\":\"kali-suroot\",\"id\":123456789,\"private\":false,\"desc\":null}";
    lg_json_value *root = NULL;
    long long id = 0;
    int priv = -1;

    LG_TEST_ASSERT(lg_json_parse(j, strlen(j), &root) == 0);
    LG_TEST_ASSERT(root != NULL && root->type == LG_JSON_OBJECT);
    LG_TEST_ASSERT_STREQ(lg_json_get_string(root, "login"), "kali-suroot");
    LG_TEST_ASSERT(lg_json_int64(lg_json_get(root, "id"), &id) == 0);
    LG_TEST_ASSERT(id == 123456789LL);
    LG_TEST_ASSERT(lg_json_bool(lg_json_get(root, "private"), &priv) == 0);
    LG_TEST_ASSERT(priv == 0);
    LG_TEST_ASSERT(lg_json_get(root, "desc") != NULL);
    LG_TEST_ASSERT(lg_json_get(root, "desc")->type == LG_JSON_NULL);
    LG_TEST_ASSERT(lg_json_get(root, "nonexistent") == NULL);
    lg_json_free(root);
    return 0;
}

static int LgTestJsonArray(void)
{
    const char *j = "[{\"name\":\"a\"},{\"name\":\"b\"},{\"name\":\"c\"}]";
    lg_json_value *root = NULL;

    LG_TEST_ASSERT(lg_json_parse(j, strlen(j), &root) == 0);
    LG_TEST_ASSERT(root != NULL && root->type == LG_JSON_ARRAY);
    LG_TEST_ASSERT(root->count == 3);
    LG_TEST_ASSERT_STREQ(lg_json_get_string(lg_json_at(root, 0), "name"), "a");
    LG_TEST_ASSERT_STREQ(lg_json_get_string(lg_json_at(root, 1), "name"), "b");
    LG_TEST_ASSERT_STREQ(lg_json_get_string(lg_json_at(root, 2), "name"), "c");
    LG_TEST_ASSERT(lg_json_at(root, 3) == NULL);
    lg_json_free(root);
    return 0;
}

static int LgTestJsonStringEscapes(void)
{
    const char *j = "{\"s\":\"a\\n\\t\\u4e2d\\\"b\"}";
    lg_json_value *root = NULL;

    LG_TEST_ASSERT(lg_json_parse(j, strlen(j), &root) == 0);
    LG_TEST_ASSERT_STREQ(lg_json_get_string(root, "s"), "a\n\t\xe4\xb8\xad\"b");
    lg_json_free(root);
    return 0;
}

static int LgTestJsonNested(void)
{
    const char *j =
        "{\"tree\":[{\"path\":\"src/a.c\",\"mode\":\"100644\",\"type\":\"blob\",\"sha\":\"abc123\"},"
        "{\"path\":\"src\",\"mode\":\"040000\",\"type\":\"tree\",\"sha\":\"def456\"}],\"truncated\":false}";
    lg_json_value *root = NULL;
    lg_json_value *tree, *first, *second;
    int truncated = -1;

    LG_TEST_ASSERT(lg_json_parse(j, strlen(j), &root) == 0);
    tree = lg_json_get(root, "tree");
    LG_TEST_ASSERT(tree != NULL && tree->type == LG_JSON_ARRAY && tree->count == 2);

    first = lg_json_at(tree, 0);
    second = lg_json_at(tree, 1);
    LG_TEST_ASSERT_STREQ(lg_json_get_string(first, "path"), "src/a.c");
    LG_TEST_ASSERT_STREQ(lg_json_get_string(first, "mode"), "100644");
    LG_TEST_ASSERT_STREQ(lg_json_get_string(first, "type"), "blob");
    LG_TEST_ASSERT_STREQ(lg_json_get_string(first, "sha"), "abc123");
    LG_TEST_ASSERT_STREQ(lg_json_get_string(second, "type"), "tree");

    LG_TEST_ASSERT(lg_json_bool(lg_json_get(root, "truncated"), &truncated) == 0);
    LG_TEST_ASSERT(truncated == 0);

    lg_json_free(root);
    return 0;
}

static int LgTestJsonInvalid(void)
{
    lg_json_value *root = NULL;
    LG_TEST_ASSERT(lg_json_parse("{", 1, &root) != 0);
    LG_TEST_ASSERT(lg_json_parse("", 0, &root) != 0);
    LG_TEST_ASSERT(lg_json_parse("[1,2", 4, &root) != 0);
    return 0;
}

int lg_run_json_tests(void)
{
    LG_TEST_ASSERT(LgTestJsonBasic() == 0);
    LG_TEST_ASSERT(LgTestJsonArray() == 0);
    LG_TEST_ASSERT(LgTestJsonStringEscapes() == 0);
    LG_TEST_ASSERT(LgTestJsonNested() == 0);
    LG_TEST_ASSERT(LgTestJsonInvalid() == 0);
    return 0;
}
