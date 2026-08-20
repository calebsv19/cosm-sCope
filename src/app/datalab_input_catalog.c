#include "app/datalab_input_catalog.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "data/input_file_loader.h"

enum { DATALAB_INPUT_CATALOG_INITIAL_CAPACITY = 64, DATALAB_INPUT_CATALOG_SORT_RUN_SIZE = 512, DATALAB_INPUT_CATALOG_SYNC_STEP = 512 };
static const DatalabInputCatalog *g_sort_catalog = NULL;
static int g_fail_next_allocation = 0;

static uint64_t now_us(void) { struct timeval now = {0}; return gettimeofday(&now, NULL) == 0 ? (uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_usec : 0u; }
static int grow(void **p, size_t *cap, size_t need, size_t unit) {
    size_t next = *cap ? *cap : DATALAB_INPUT_CATALOG_INITIAL_CAPACITY; void *replacement = NULL;
    if (need <= *cap) return 1;
    while (next < need) { if (next > SIZE_MAX / 2u) return 0; next *= 2u; }
    if (next > SIZE_MAX / unit) return 0;
    if (g_fail_next_allocation) { g_fail_next_allocation = 0; return 0; }
    if (!(replacement = core_alloc(next * unit))) return 0;
    if (*p) memcpy(replacement, *p, *cap * unit);
    core_free(*p); *p = replacement; *cap = next; return 1;
}
static uint64_t fingerprint(const struct stat *st) {
    uint64_t h = 1469598103934665603ull;
#if defined(__APPLE__)
    const uint64_t mn = (uint64_t)st->st_mtimespec.tv_nsec, cn = (uint64_t)st->st_ctimespec.tv_nsec;
#elif defined(__linux__)
    const uint64_t mn = (uint64_t)st->st_mtim.tv_nsec, cn = (uint64_t)st->st_ctim.tv_nsec;
#else
    const uint64_t mn = 0u, cn = 0u;
#endif
    const uint64_t values[] = {(uint64_t)st->st_dev,(uint64_t)st->st_ino,(uint64_t)st->st_mtime,mn,(uint64_t)st->st_ctime,cn,(uint64_t)st->st_nlink,(uint64_t)st->st_size};
    for (size_t i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) { h ^= values[i]; h *= 1099511628211ull; } return h;
}
static int read_fingerprint(const char *root, uint64_t *out) { struct stat st = {0}; if (!root || !out || stat(root, &st) || !S_ISDIR(st.st_mode)) return 0; *out = fingerprint(&st); return 1; }
static int natural_cmp(const char *left, const char *right) {
    const unsigned char *a = (const unsigned char *)left, *b = (const unsigned char *)right;
    while (*a && *b) {
        if (isdigit(*a) && isdigit(*b)) {
            const unsigned char *as=a,*bs=b,*ae=a,*be=b,*az,*bz; while (*as=='0') ++as; while (*bs=='0') ++bs; while (isdigit(*ae)) ++ae; while (isdigit(*be)) ++be; az=as; bz=bs; while (isdigit(*az)) ++az; while (isdigit(*bz)) ++bz;
            if ((size_t)(az-as)!=(size_t)(bz-bs)) return az-as < bz-bs ? -1 : 1;
            if (az!=as) { int c=memcmp(as,bs,(size_t)(az-as)); if (c) return c; }
            if ((size_t)(ae-a)!=(size_t)(be-b)) return ae-a < be-b ? -1 : 1;
            a=ae; b=be; continue;
        }
        { int af=tolower(*a), bf=tolower(*b); if (af!=bf) return af<bf?-1:1; } ++a; ++b;
    }
    if (*a != *b) return *a ? 1 : -1; return strcasecmp(left, right);
}
static const char *scan_name(const DatalabInputCatalog *c, uint64_t entry) { return c && entry < c->scan_count ? c->scan_names + c->scan_entries[entry].name_offset : ""; }
static int index_cmp(const void *a, const void *b) { return natural_cmp(scan_name(g_sort_catalog, *(const uint64_t *)a), scan_name(g_sort_catalog, *(const uint64_t *)b)); }
static int sequence_number(const char *name,char *prefix,size_t prefix_cap,unsigned long long *number,char *suffix,size_t suffix_cap){const char *start=NULL,*end=NULL;char *parse_end=NULL;if(!name||!prefix||!number||!suffix)return 0;for(const char *p=name;*p;++p)if(isdigit((unsigned char)*p)){start=p;break;}if(!start)return 0;end=start;while(isdigit((unsigned char)*end))++end;if((size_t)(start-name)>=prefix_cap||strlen(end)>=suffix_cap)return 0;memcpy(prefix,name,(size_t)(start-name));prefix[start-name]='\0';memcpy(suffix,end,strlen(end)+1u);*number=strtoull(start,&parse_end,10);return parse_end==end;}

static void release_scan(DatalabInputCatalog *c) {
    if (!c) return; if (c->scan_dir) closedir(c->scan_dir);
    for (size_t i=0u;i<c->scan_run_count;++i) core_free(c->scan_runs[i].indexes);
    core_free(c->scan_entries); core_free(c->scan_names); core_free(c->scan_root); core_free(c->scan_order); core_free(c->scan_runs); core_free(c->merge_heap); core_free(c->merge_positions);
    c->scan_dir=NULL; c->scan_entries=NULL; c->scan_names=NULL; c->scan_root=NULL; c->scan_order=NULL; c->scan_runs=NULL; c->merge_heap=NULL; c->merge_positions=NULL;
    c->scan_count=c->scan_entry_capacity=c->scan_name_bytes=c->scan_name_capacity=c->scan_order_capacity=c->scan_run_count=c->scan_run_capacity=c->scan_run_fill=c->merge_heap_count=c->merge_output_count=0u;
}
static void update_peak(DatalabInputCatalog *c) {
    size_t bytes=0u; if (!c) return;
    bytes = c->entry_capacity*sizeof(*c->entries)+c->file_count*sizeof(*c->order)+c->name_capacity + c->scan_entry_capacity*sizeof(*c->scan_entries)+c->scan_name_capacity+c->scan_order_capacity*sizeof(*c->scan_order);
    for (size_t i=0u;i<c->scan_run_count;++i) bytes += c->scan_runs[i].count*sizeof(uint64_t);
    if (bytes > c->metrics.peak_bytes) c->metrics.peak_bytes=bytes;
}
static CoreResult append_name(DatalabInputCatalog *c, const char *name) {
    size_t len=0u; if (!c || !name) return (CoreResult){CORE_ERR_INVALID_ARG,"invalid catalog entry"}; len=strlen(name);
    if (!len || len>UINT32_MAX || c->scan_name_bytes>SIZE_MAX-len-1u) return (CoreResult){CORE_ERR_INVALID_ARG,"catalog name overflow"};
    if (!grow((void **)&c->scan_entries,&c->scan_entry_capacity,c->scan_count+1u,sizeof(*c->scan_entries)) || !grow((void **)&c->scan_names,&c->scan_name_capacity,c->scan_name_bytes+len+1u,sizeof(char)) || !grow((void **)&c->scan_order,&c->scan_order_capacity,c->scan_count+1u,sizeof(*c->scan_order))) return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"input catalog allocation failed"};
    c->scan_entries[c->scan_count].name_offset=c->scan_name_bytes; c->scan_entries[c->scan_count].name_length=(uint32_t)len; memcpy(c->scan_names+c->scan_name_bytes,name,len+1u); c->scan_name_bytes+=len+1u; c->scan_order[c->scan_count]=c->scan_count; ++c->scan_count; ++c->scan_run_fill; return core_result_ok();
}
static CoreResult finish_run(DatalabInputCatalog *c) {
    DatalabInputCatalogRun *run=NULL; size_t start=0u; if (!c || !c->scan_run_fill) return core_result_ok(); start=c->scan_count-c->scan_run_fill;
    if (!grow((void **)&c->scan_runs,&c->scan_run_capacity,c->scan_run_count+1u,sizeof(*c->scan_runs))) return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"catalog sort-run allocation failed"};
    run=&c->scan_runs[c->scan_run_count]; run->indexes=core_alloc(c->scan_run_fill*sizeof(*run->indexes)); if (!run->indexes) return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"catalog sort-run allocation failed"}; run->count=c->scan_run_fill; memcpy(run->indexes,c->scan_order+start,run->count*sizeof(*run->indexes));
    g_sort_catalog=c; qsort(run->indexes,run->count,sizeof(*run->indexes),index_cmp); g_sort_catalog=NULL; ++c->scan_run_count; c->scan_run_fill=0u; c->metrics.sorted_run_count=c->scan_run_count; return core_result_ok();
}
static int heap_less(const DatalabInputCatalog *c, size_t left, size_t right) { const DatalabInputCatalogRun *a=&c->scan_runs[left],*b=&c->scan_runs[right]; return natural_cmp(scan_name(c,a->indexes[c->merge_positions[left]]),scan_name(c,b->indexes[c->merge_positions[right]]))<0; }
static void heap_down(DatalabInputCatalog *c, size_t at) { for (;;) { size_t child=at*2u+1u; if (child>=c->merge_heap_count) return; if (child+1u<c->merge_heap_count && heap_less(c,c->merge_heap[child+1u],c->merge_heap[child])) ++child; if (heap_less(c,c->merge_heap[at],c->merge_heap[child])) return; { size_t tmp=c->merge_heap[at];c->merge_heap[at]=c->merge_heap[child];c->merge_heap[child]=tmp;} at=child; } }
static CoreResult begin_merge(DatalabInputCatalog *c) {
    CoreResult r=finish_run(c); if (r.code!=CORE_OK) return r;
    if (c->scan_count) { core_free(c->scan_order); c->scan_order=core_alloc(c->scan_count*sizeof(*c->scan_order)); c->merge_heap=core_alloc(c->scan_run_count*sizeof(*c->merge_heap)); c->merge_positions=core_alloc(c->scan_run_count*sizeof(*c->merge_positions)); if (!c->scan_order || !c->merge_heap || !c->merge_positions) return (CoreResult){CORE_ERR_OUT_OF_MEMORY,"catalog merge allocation failed"}; memset(c->merge_positions,0,c->scan_run_count*sizeof(*c->merge_positions)); for (size_t i=0u;i<c->scan_run_count;++i) c->merge_heap[i]=i; c->merge_heap_count=c->scan_run_count; for (size_t i=c->merge_heap_count/2u;i>0u;--i) heap_down(c,i-1u); }
    c->state=DATALAB_INPUT_CATALOG_MERGING; return core_result_ok();
}
static CoreResult publish(DatalabInputCatalog *c) {
    uint64_t fp=0u;
    if (!c || !c->scan_root ||
        (strncmp(c->scan_root, "fixture://", sizeof("fixture://") - 1u) != 0 &&
         !read_fingerprint(c->scan_root,&fp))) return (CoreResult){CORE_ERR_IO,"input root unavailable"};
    core_free(c->entries);core_free(c->order);core_free(c->names);core_free(c->root); c->entries=c->scan_entries;c->entry_capacity=c->scan_entry_capacity;c->names=c->scan_names;c->name_capacity=c->scan_name_capacity;c->name_bytes=c->scan_name_bytes;c->order=c->scan_order;c->file_count=c->scan_count;c->root=c->scan_root;c->directory_fingerprint=fp;c->fingerprint_valid=1;c->last_scan_duration_us=now_us()-c->scan_started_us;++c->refresh_count;if(++c->generation==0u)c->generation=1u;
    c->sequence_gap_count=0u;for(size_t i=1u;i<c->file_count;++i){char left[DATALAB_APP_PATH_CAP],right[DATALAB_APP_PATH_CAP],lp[DATALAB_APP_PATH_CAP],rp[DATALAB_APP_PATH_CAP],ls[DATALAB_APP_PATH_CAP],rs[DATALAB_APP_PATH_CAP];unsigned long long ln=0u,rn=0u;if(!datalab_input_catalog_name_copy(c,i-1u,left,sizeof(left))||!datalab_input_catalog_name_copy(c,i,right,sizeof(right))||!sequence_number(left,lp,sizeof(lp),&ln,ls,sizeof(ls))||!sequence_number(right,rp,sizeof(rp),&rn,rs,sizeof(rs))||strcmp(lp,rp)||strcmp(ls,rs)||rn<=ln+1u)continue;c->sequence_gap_count+=(size_t)(rn-ln-1u);}
    c->metrics.metadata_bytes=c->file_count*(sizeof(*c->entries)+sizeof(*c->order));c->metrics.name_bytes=c->name_bytes;c->scan_entries=NULL;c->scan_names=NULL;c->scan_order=NULL;c->scan_root=NULL;c->scan_entry_capacity=c->scan_name_capacity=c->scan_count=c->scan_name_bytes=0u;release_scan(c);c->state=DATALAB_INPUT_CATALOG_READY;update_peak(c);return core_result_ok();
}

void datalab_input_catalog_init(DatalabInputCatalog *c) { if (c) memset(c,0,sizeof(*c)); }
void datalab_input_catalog_clear(DatalabInputCatalog *c) { if (!c) return; release_scan(c);core_free(c->entries);core_free(c->order);core_free(c->names);core_free(c->root);datalab_input_catalog_init(c); }
void datalab_input_catalog_destroy(DatalabInputCatalog *c) { datalab_input_catalog_clear(c); }
void datalab_input_catalog_test_fail_next_allocation(void) { g_fail_next_allocation = 1; }
CoreResult datalab_input_catalog_begin_refresh(DatalabInputCatalog *c,const char *root,DatalabInputCatalogRefreshReason reason) { size_t len=0u; if(!c||!root||!root[0])return(CoreResult){CORE_ERR_INVALID_ARG,"invalid input catalog refresh request"};release_scan(c);c->last_refresh_reason=reason;c->state=DATALAB_INPUT_CATALOG_ERROR;len=strlen(root);c->scan_root=core_alloc(len+1u);if(!c->scan_root)return(CoreResult){CORE_ERR_OUT_OF_MEMORY,"input catalog root allocation failed"};memcpy(c->scan_root,root,len+1u);c->scan_dir=opendir(root);if(!c->scan_dir){core_free(c->scan_root);c->scan_root=NULL;core_free(c->entries);core_free(c->order);core_free(c->names);core_free(c->root);c->entries=NULL;c->order=NULL;c->names=NULL;c->root=NULL;c->file_count=c->entry_capacity=c->name_bytes=c->name_capacity=0u;c->fingerprint_valid=0;c->directory_fingerprint=0u;return(CoreResult){CORE_ERR_IO,"input root unavailable"};}c->scan_started_us=now_us();c->state=DATALAB_INPUT_CATALOG_SCANNING;return core_result_ok(); }
CoreResult datalab_input_catalog_step(DatalabInputCatalog *c,size_t limit) {
    if(!c||!limit)return(CoreResult){CORE_ERR_INVALID_ARG,"invalid catalog step request"};
    if(c->state==DATALAB_INPUT_CATALOG_SCANNING){size_t seen=0u;while(seen<limit){struct dirent *e=readdir(c->scan_dir);if(!e){closedir(c->scan_dir);c->scan_dir=NULL;return begin_merge(c);}++seen;if(e->d_name[0]=='.'||!datalab_input_file_is_supported(e->d_name))continue;CoreResult r=append_name(c,e->d_name);if(r.code!=CORE_OK){c->state=DATALAB_INPUT_CATALOG_ERROR;return r;}if(c->scan_run_fill==DATALAB_INPUT_CATALOG_SORT_RUN_SIZE&&(r=finish_run(c)).code!=CORE_OK){c->state=DATALAB_INPUT_CATALOG_ERROR;return r;}}if(seen>c->metrics.max_step_directory_entries)c->metrics.max_step_directory_entries=seen;update_peak(c);return core_result_ok();}
    if(c->state==DATALAB_INPUT_CATALOG_MERGING){size_t made=0u;while(made<limit&&c->merge_heap_count){size_t run_index=c->merge_heap[0],last=0u;DatalabInputCatalogRun *run=&c->scan_runs[run_index];c->scan_order[c->merge_output_count++]=run->indexes[c->merge_positions[run_index]++];++made;if(c->merge_positions[run_index]==run->count){last=--c->merge_heap_count;c->merge_heap[0]=c->merge_heap[last];}if(c->merge_heap_count)heap_down(c,0u);}if(made>c->metrics.max_step_merge_entries)c->metrics.max_step_merge_entries=made;return c->merge_heap_count?core_result_ok():publish(c);}return c->state==DATALAB_INPUT_CATALOG_READY?core_result_ok():(CoreResult){CORE_ERR_IO,"catalog is not active"};
}
void datalab_input_catalog_cancel(DatalabInputCatalog *c){if(c&&datalab_input_catalog_is_busy(c)){release_scan(c);c->state=DATALAB_INPUT_CATALOG_CANCELED;}}
int datalab_input_catalog_is_busy(const DatalabInputCatalog *c){return c&&(c->state==DATALAB_INPUT_CATALOG_SCANNING||c->state==DATALAB_INPUT_CATALOG_MERGING);}
int datalab_input_catalog_is_ready(const DatalabInputCatalog *c){return c&&c->state==DATALAB_INPUT_CATALOG_READY;}
const char *datalab_input_catalog_state_name(DatalabInputCatalogState s){switch(s){case DATALAB_INPUT_CATALOG_SCANNING:return"scanning";case DATALAB_INPUT_CATALOG_MERGING:return"merging";case DATALAB_INPUT_CATALOG_READY:return"ready";case DATALAB_INPUT_CATALOG_CANCELED:return"canceled";case DATALAB_INPUT_CATALOG_ERROR:return"error";default:return"idle";}}
CoreResult datalab_input_catalog_refresh(DatalabInputCatalog *c,const char *root,DatalabInputCatalogRefreshReason reason){CoreResult r=datalab_input_catalog_begin_refresh(c,root,reason);while(r.code==CORE_OK&&datalab_input_catalog_is_busy(c))r=datalab_input_catalog_step(c,DATALAB_INPUT_CATALOG_SYNC_STEP);return r;}
CoreResult datalab_input_catalog_scan(DatalabInputCatalog *c,const char *root){return datalab_input_catalog_refresh(c,root,DATALAB_INPUT_CATALOG_REFRESH_INITIAL);}
size_t datalab_input_catalog_count(const DatalabInputCatalog *c){return c?c->file_count:0u;}
const char *datalab_input_catalog_root(const DatalabInputCatalog *c){return c&&c->root?c->root:"";}
const DatalabInputCatalogMetrics *datalab_input_catalog_metrics(const DatalabInputCatalog *c){return c?&c->metrics:NULL;}
int datalab_input_catalog_name_copy(const DatalabInputCatalog *c,uint64_t index,char *out,size_t cap){uint64_t raw=0u;const DatalabInputCatalogEntry *entry=NULL;if(!c||!out||!cap||index>=c->file_count)return 0;raw=c->order[index];entry=&c->entries[raw];if((size_t)entry->name_length>=cap)return 0;memcpy(out,c->names+entry->name_offset,(size_t)entry->name_length+1u);return 1;}
size_t datalab_input_catalog_page_copy(const DatalabInputCatalog *c,uint64_t first,char (*out)[DATALAB_APP_PATH_CAP],size_t cap){size_t n=0u;if(!c||!out)return 0u;while(n<cap&&first+n<c->file_count&&datalab_input_catalog_name_copy(c,first+n,out[n],DATALAB_APP_PATH_CAP))++n;return n;}
int datalab_input_catalog_find_index(const DatalabInputCatalog *c,const char *name,uint64_t *out){char candidate[DATALAB_APP_PATH_CAP];if(!c||!name||!name[0]||!out)return 0;for(size_t i=0u;i<c->file_count;++i)if(datalab_input_catalog_name_copy(c,i,candidate,sizeof(candidate))&&strcasecmp(candidate,name)==0){*out=(uint64_t)i;return 1;}return 0;}
int datalab_input_catalog_find_name(const DatalabInputCatalog *c,const char *name){uint64_t found=0u;return datalab_input_catalog_find_index(c,name,&found)&&found<=INT_MAX?(int)found:-1;}
int datalab_input_catalog_restore_selection(const DatalabInputCatalog *c,const char *name,uint64_t previous,uint64_t *out){uint64_t found=0u;if(!c||!out||!c->file_count)return 0;*out=datalab_input_catalog_find_index(c,name,&found)?found:(previous<c->file_count?previous:c->file_count-1u);return 1;}
int datalab_input_catalog_root_matches(const DatalabInputCatalog *c,const char *root){return c&&root&&root[0]&&c->root&&strcmp(c->root,root)==0;}
int datalab_input_catalog_fingerprint_changed(const DatalabInputCatalog *c,const char *root){uint64_t fp=0u;return !c||!datalab_input_catalog_root_matches(c,root)||!c->fingerprint_valid||!read_fingerprint(root,&fp)||fp!=c->directory_fingerprint;}
int datalab_input_catalog_file_is_current(const DatalabInputCatalog *c,const char *root,const char *name){char path[DATALAB_APP_PATH_CAP];struct stat st={0};return datalab_input_catalog_root_matches(c,root)&&datalab_input_catalog_find_name(c,name)>=0&&datalab_input_root_join_child_file(root,name,path,sizeof(path))&&stat(path,&st)==0&&S_ISREG(st.st_mode);}
const char *datalab_input_catalog_refresh_reason_name(DatalabInputCatalogRefreshReason r){switch(r){case DATALAB_INPUT_CATALOG_REFRESH_INITIAL:return"initial";case DATALAB_INPUT_CATALOG_REFRESH_ROOT_CHANGE:return"root-change";case DATALAB_INPUT_CATALOG_REFRESH_EXPLICIT:return"explicit";case DATALAB_INPUT_CATALOG_REFRESH_FINGERPRINT_CHANGED:return"fingerprint-change";default:return"none";}}
CoreResult datalab_input_catalog_generate_fixture(DatalabInputCatalog *c,size_t count){
    CoreResult r=core_result_ok();char name[64];
    if(!c)return(CoreResult){CORE_ERR_INVALID_ARG,"invalid catalog fixture request"};
    release_scan(c);c->scan_root=core_alloc(sizeof("fixture://logical"));if(!c->scan_root)return(CoreResult){CORE_ERR_OUT_OF_MEMORY,"input catalog root allocation failed"};strcpy(c->scan_root,"fixture://logical");c->scan_started_us=now_us();c->last_refresh_reason=DATALAB_INPUT_CATALOG_REFRESH_EXPLICIT;c->state=DATALAB_INPUT_CATALOG_SCANNING;
    for(size_t i=0u;i<count;++i){snprintf(name,sizeof(name),"frame_%010zu.png",count-i);if((r=append_name(c,name)).code!=CORE_OK){c->state=DATALAB_INPUT_CATALOG_ERROR;return r;}if(c->scan_run_fill==DATALAB_INPUT_CATALOG_SORT_RUN_SIZE&&(r=finish_run(c)).code!=CORE_OK){c->state=DATALAB_INPUT_CATALOG_ERROR;return r;}}
    if((r=begin_merge(c)).code!=CORE_OK){c->state=DATALAB_INPUT_CATALOG_ERROR;return r;}while(datalab_input_catalog_is_busy(c)&&(r=datalab_input_catalog_step(c,DATALAB_INPUT_CATALOG_SYNC_STEP)).code==CORE_OK){}return r;
}
