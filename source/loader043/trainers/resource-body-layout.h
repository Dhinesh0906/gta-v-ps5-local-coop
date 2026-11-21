#ifndef GTA_RESOURCE_BODY_LAYOUT_H
#define GTA_RESOURCE_BODY_LAYOUT_H
/* A body device skips the RSC7 header in OpenBulk and must expose the same
 * header-free length to the streaming metadata query. Failure stays failure.
 */
static inline unsigned long long gr_resource_body_length(unsigned long long physical){
 return physical==~0ULL||physical<16?~0ULL:physical-16;
}
#endif
