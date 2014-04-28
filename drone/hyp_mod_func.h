void * pos_modulelist(MODULE_LIST * moduleList,uint32_t * mid);
int mod_remote_install(const char *buff);
int mod_drone(const char *buff);
int mod_insert_list(MODULE_LIST * moduleList,uint32_t * mid,void * funcAddr,void *);
int mod_loc_loader(char * soName);