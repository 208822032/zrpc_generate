// zrpc_generate.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// 生成zrpc_method.h文件
void generate_zrpc_method_h(const char *filename, cJSON *config) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "#ifndef __ZRPC_METHOD__\n");
    fprintf(fp, "#define __ZRPC_METHOD__\n\n");

    cJSON *item;
    cJSON_ArrayForEach(item, config) {
        cJSON *method = cJSON_GetObjectItem(item, "method");
        cJSON *rettype = cJSON_GetObjectItem(item, "rettype");
        fprintf(fp, "%s %s(", rettype->valuestring, method->valuestring);

        cJSON *params = cJSON_GetObjectItem(item, "params");
        cJSON *types = cJSON_GetObjectItem(item, "types");
        int param_count = cJSON_GetArraySize(params);

        for (int i = 0; i < param_count; i++) {
            cJSON *param = cJSON_GetArrayItem(params, i);
            cJSON *type = cJSON_GetArrayItem(types, i);
            fprintf(fp, "%s %s", type->valuestring, param->valuestring);
            if (i < param_count - 1) fprintf(fp, ", ");
        }
        fprintf(fp, ");\n");
    }

    fprintf(fp, "\n#endif\n");
    fclose(fp);
    printf("Generated: %s\n", filename);
}

// 生成zrpc_method.c文件
void generate_zrpc_method_c(const char *filename, cJSON *config) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "#include \"zrpc.h\"\n");
    fprintf(fp, "#include \"zrpc_method.h\"\n\n");

    cJSON *item;
    cJSON_ArrayForEach(item, config) {
        cJSON *method = cJSON_GetObjectItem(item, "method");
        cJSON *rettype = cJSON_GetObjectItem(item, "rettype");
        cJSON *params = cJSON_GetObjectItem(item, "params");
        cJSON *types = cJSON_GetObjectItem(item, "types");
        int param_count = cJSON_GetArraySize(params);

        // 生成客户端存根函数
        fprintf(fp, "%s %s(", rettype->valuestring, method->valuestring);
        for (int i = 0; i < param_count; i++) {
            cJSON *param = cJSON_GetArrayItem(params, i);
            cJSON *type = cJSON_GetArrayItem(types, i);
            fprintf(fp, "%s %s", type->valuestring, param->valuestring);
            if (i < param_count - 1) fprintf(fp, ", ");
        }
        fprintf(fp, ") {\n");
        fprintf(fp, "    char *request = zrpc_request_json_encode(%d", param_count);
        for (int i = 0; i < param_count; i++) {
            cJSON *param = cJSON_GetArrayItem(params, i);
            fprintf(fp, ", %s", param->valuestring);
        }
        fprintf(fp, ");\n");
        fprintf(fp, "    char *response = zrpc_client_session(request);\n");
        fprintf(fp, "    char *result = zrpc_response_json_decode(response);\n\n");
        
        if (strcmp(rettype->valuestring, "char *") == 0) {
            fprintf(fp, "    char *ret = strdup(result);\n");
        } else if (strcmp(rettype->valuestring, "int") == 0) {
            fprintf(fp, "    int ret = atoi(result);\n");
        } else if (strcmp(rettype->valuestring, "float") == 0) {
            fprintf(fp, "    float ret = (float)atof(result);\n");
        } else if (strcmp(rettype->valuestring, "double") == 0) {
            fprintf(fp, "    double ret = atof(result);\n");
        }
        
        fprintf(fp, "    free(result);\n");
        fprintf(fp, "    free(response);\n");
        fprintf(fp, "    free(request);\n");
        fprintf(fp, "    return ret;\n");
        fprintf(fp, "}\n\n");

        // 生成服务端处理函数声明
        fprintf(fp, "char *zrpc_response_json_encode_%s(cJSON *params, struct zrpc_task *task) {\n", method->valuestring);
        for (int i = 0; i < param_count; i++) {
            cJSON *param = cJSON_GetArrayItem(params, i);
            cJSON *type = cJSON_GetArrayItem(types, i);
            if (strcmp(type->valuestring, "int") == 0) {
                fprintf(fp, "    cJSON *cjson_%s = cJSON_GetObjectItem(params, \"%s\");\n", param->valuestring, param->valuestring);
                fprintf(fp, "    int %s = cjson_%s->valueint;\n", param->valuestring, param->valuestring);
            } else if (strcmp(type->valuestring, "float") == 0 || 
                       strcmp(type->valuestring, "double") == 0) {
                fprintf(fp, "    cJSON *cjson_%s = cJSON_GetObjectItem(params, \"%s\");\n", param->valuestring, param->valuestring);
                fprintf(fp, "    %s %s = cjson_%s->valuedouble;\n", type->valuestring, param->valuestring, param->valuestring);
            } else if (strcmp(type->valuestring, "char *") == 0) {
                fprintf(fp, "    cJSON *cjson_%s = cJSON_GetObjectItem(params, \"%s\");\n", param->valuestring, param->valuestring);
                fprintf(fp, "    char *%s = cjson_%s->valuestring;\n", param->valuestring, param->valuestring);
            }
        }
        fprintf(fp, "    %s ret = zrpc_%s(", rettype->valuestring, method->valuestring);
        for (int i = 0; i < param_count; i++) {
            cJSON *param = cJSON_GetArrayItem(params, i);
            fprintf(fp, "%s", param->valuestring);
            if (i < param_count - 1) fprintf(fp, ", ");
        }
        fprintf(fp, ");\n\n");
        fprintf(fp, "    cJSON *root = cJSON_CreateObject();\n");
        fprintf(fp, "    cJSON_AddStringToObject(root, \"method\", task->method);\n");
        if (strcmp(rettype->valuestring, "char *") == 0) {
            fprintf(fp, "    cJSON_AddStringToObject(root, \"result\", ret);\n");
        } else {
            fprintf(fp, "    cJSON_AddNumberToObject(root, \"result\", ret);\n");
        }
        fprintf(fp, "    cJSON_AddNumberToObject(root, \"callerid\", task->callerid);\n");
        fprintf(fp, "    char *out = cJSON_Print(root);\n");
        fprintf(fp, "    cJSON_Delete(root);\n");
        fprintf(fp, "    return out;\n");
        fprintf(fp, "}\n\n");
    }

    fclose(fp);
    printf("Generated: %s\n", filename);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <register.json> <zrpc_method.c> <zrpc_method.h>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 读取register.json文件
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *json_data = malloc(length + 1);
    fread(json_data, 1, length, fp);
    json_data[length] = '\0';
    fclose(fp);

    // 解析JSON
    cJSON *json = cJSON_Parse(json_data);
    if (!json) {
        fprintf(stderr, "Error parsing JSON\n");
        free(json_data);
        return EXIT_FAILURE;
    }

    // 获取config数组
    cJSON *config = cJSON_GetObjectItemCaseSensitive(json, "config");
    if (!cJSON_IsArray(config)) {
        fprintf(stderr, "Error: 'config' is not an array\n");
        cJSON_Delete(json);
        free(json_data);
        return EXIT_FAILURE;
    }

    // 生成文件
    generate_zrpc_method_c(argv[2], config);
    generate_zrpc_method_h(argv[3], config);

    // 清理
    cJSON_Delete(json);
    free(json_data);
    return EXIT_SUCCESS;
}
