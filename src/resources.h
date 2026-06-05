#ifndef RESOURCES_H
#define RESOURCES_H

#include <stddef.h>

typedef struct {
    const char *name;
    const char *title;
    const char *description;
    const char *uri;
    const char *mimeType;
    const char *text;
} mcp_resource_t;

int register_resource(const mcp_resource_t *resource);
const mcp_resource_t *find_resource(const char *uri);
size_t get_registered_resource_count(void);
const mcp_resource_t *get_registered_resource(size_t index);

void register_default_mcp_resources(void);
void register_default_resource_methods(void);

#endif // RESOURCES_H
