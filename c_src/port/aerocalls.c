/* aerocalls.c */

#include "ei.h"
#include <string.h>
#include <stdlib.h>

#include <aerospike/aerospike.h>
#include <aerospike/aerospike_info.h>
#include <aerospike/aerospike_key.h>
#include <aerospike/as_error.h>
#include <aerospike/as_record.h>
#include <aerospike/as_record_iterator.h>
#include <aerospike/as_status.h>
#include <aerospike/as_node.h>
#include <aerospike/as_cluster.h>
#include <aerospike/as_lookup.h>

// ----------------------------------------------------------------------------

#define MAX_HOST_SIZE 1024
#define MAX_KEY_STR_SIZE 1024
#define MAX_NAMESPACE_SIZE 32	// based on current server limit
#define MAX_SET_SIZE 64			// based on current server limit
#define AS_BIN_NAME_MAX_SIZE 16
// #define AS_NODE_NAME_MAX_SIZE 16

const char DEFAULT_HOST[] = "127.0.0.1";
// const int DEFAULT_PORT = 3000;
const int DEFAULT_PORT = 3010;
const char DEFAULT_NAMESPACE[] = "test";
const char DEFAULT_SET[] = "eg-set";
const char DEFAULT_KEY_STR[] = "eg-key";
const uint32_t DEFAULT_NUM_KEYS = 20;

// ----------------------------------------------------------------------------

#define OK0\
    res = 1;\
    ei_x_encode_atom(&res_buf, "ok");
#define OK(MSG)\
    OK0\
    ei_x_encode_string(&res_buf, MSG);

#define OK0P\
    res = 1;\
    ei_x_encode_atom(p_res_buf, "ok");
#define OKP(MSG)\
    OK0P\
    ei_x_encode_string(p_res_buf, MSG);


#define ERROR0\
    res = 0;\
    ei_x_encode_atom(&res_buf, "error");
#define ERROR(MSG)\
    ERROR0\
    ei_x_encode_string(&res_buf, MSG);
#define ERROR0P\
    res = 0;\
    ei_x_encode_atom(p_res_buf, "error");
#define ERRORP(MSG)\
    ERROR0P\
    ei_x_encode_string(p_res_buf, MSG);

#define PRE \
    int res = 1;\
    ei_x_buff res_buf;\
    ei_x_new_with_version(&res_buf);\
    ei_x_encode_tuple_header(&res_buf, 2);

#define POST \
    write_cmd(res_buf.buff, res_buf.index, fd_out);\
    ei_x_free(&res_buf);\
    return res;

#define CHECK_AEROSPIKE_INIT \
    if (!is_aerospike_initialised) {\
        ERROR("aerospike is not initialise")\
        goto end;\
    }

#define CHECK_IS_CONNECTED \
    if (!is_connected) {\
        ERROR("not connected")\
        goto end;\
    }

#define CHECK_INIT CHECK_AEROSPIKE_INIT

#define CHECK_ALL\
    CHECK_INIT\
    CHECK_IS_CONNECTED

// ----------------------------------------------------------------------------

static aerospike as;
int is_aerospike_initialised = 0;
int is_connected = 0;

// ----------------------------------------------------------------------------

typedef char byte;

int write_cmd(byte *buf, int len, int fd);


int ifail(int ind, int fd);
int fail(const char *msg, int fd);
int note(const char *msg, int fd);
int is_function_call(const char *buf, int *index, int *arity);
int function_call(const char *buf, int *index, int arity, int fd_out);

int call_cluster_info(const char *buf, int *index, int arity, int fd_out);
int call_config_info(const char *buf, int *index, int arity, int fd_out);

int call_connect(const char *buf, int *index, int arity, int fd_out);

int call_aerospike_init(const char *buf, int *index, int arity, int fd_out);
int call_config_add_hosts(const char *buf, int *index, int arity, int fd_out);
int call_port_status(const char *buf, int *index, int arity, int fd_out);

int call_key_put(const char *buf, int *index, int arity, int fd_out);
int call_key_remove(const char *buf, int *index, int arity, int fd_out);
int call_key_get(const char *buf, int *index, int arity, int fd_out);
int call_key_generation(const char *buf, int *index, int arity, int fd_out);

int call_node_random(const char *buf, int *index, int arity, int fd_out);
int call_node_names(const char *buf, int *index, int arity, int fd_out);
int call_node_get(const char *buf, int *index, int arity, int fd_out);
int call_node_info(const char *buf, int *index, int arity, int fd_out);

int call_host_info(const char *buf, int *index, int arity, int fd_out);

int call_help(const char *buf, int *index, int arity, int fd_out);

int call_foo(const char *buf, int *index, int arity, int fd_out);
int call_bar(const char *buf, int *index, int arity, int fd_out);

char err_msg[8][80] = {
    "ei_decode_version",
    "ei_decode_tuple_header",
    "wrong arity",
    "ei_decode_atom",
    "ei_decode_long",
    "ei_x_new_with_version",
    "ei_x_encode_long",
    "ei_x_free"
};

int fail(const char *msg, int fd_out) {
    // fprintf(stderr, "Something went wrong: %s\n", msg);
    PRE
    ERROR(msg);
    POST
}

int ifail(int ind, int fd_out) {
    return fail(err_msg[ind], fd_out);
}

int note(const char *msg, int fd_out) {
    // fprintf(stdout, "note: %s\n", msg);
    PRE
    ei_x_encode_atom(&res_buf, "note");
    ei_x_encode_string(&res_buf, msg);
    POST
}

// static void res_buf_free(ei_x_buff  *res_buf, int fd_out) {
//     if (res_buf != 0 && res_buf->buff != 0 && strncmp(res_buf->buff, "", 1) != 0) { 
//         if (ei_x_free(res_buf) != 0) {
//             ifail(7, fd_out);
//             exit(1);
//         }
//     }
// }

int is_function_call(const char *buf, int *index, int *arity) {
    int res = ei_decode_tuple_header(buf, index, arity) == 0 && arity > 0;
    return res;
}

static int check_name(const char *fname, const char *pattern, int arity, int nargs) {
    return (strncmp(fname, pattern, strlen(pattern)) == 0 && arity == nargs);
}

int function_call(const char *buf, int *index, int arity, int fd_out) {
    char fname[128];
    if (ei_decode_atom(buf, index, fname) != 0) {
        ifail(3, fd_out);
        return 0;
    }

    if (check_name(fname, "aerospike_init", arity, 1)) {
        return call_aerospike_init(buf, index, arity, fd_out);
    }
    if (check_name(fname, "host_add", arity, 3)) {
        return call_config_add_hosts(buf, index, arity, fd_out);
    }
    if (check_name(fname, "connect", arity, 3)) {
        return call_connect(buf, index, arity, fd_out);
    }
    if (check_name(fname, "key_put", arity, 6)) {
        return call_key_put(buf, index, arity, fd_out);
    }
    if (check_name(fname, "key_remove", arity, 4)) {
        return call_key_remove(buf, index, arity, fd_out);
    }
    if (check_name(fname, "key_get", arity, 4)) {
        return call_key_get(buf, index, arity, fd_out);
    }
    if (check_name(fname, "key_generation", arity, 4)) {
        return call_key_generation(buf, index, arity, fd_out);
    }
    if (check_name(fname, "node_random", arity, 1)) {
        return call_node_random(buf, index, arity, fd_out);
    }
    if (check_name(fname, "node_names", arity, 1)) {
        return call_node_names(buf, index, arity, fd_out);
    }
    if (check_name(fname, "node_get", arity, 2)) {
        return call_node_get(buf, index, arity, fd_out);
    }
    if (check_name(fname, "node_info", arity, 3)) {
        return call_node_info(buf, index, arity, fd_out);
    }
    if (check_name(fname, "host_info", arity, 4)) {
        return call_host_info(buf, index, arity, fd_out);
    }
    if (check_name(fname, "config_info", arity, 1)) {
        return call_config_info(buf, index, arity, fd_out);
    }
    if (check_name(fname, "help", arity, 2)) {
        return call_help(buf, index, arity, fd_out);
    }
    if (check_name(fname, "cluster_info", arity, 1)) {
        return call_cluster_info(buf, index, arity, fd_out);
    }
    if (check_name(fname, "port_status", arity, 1)) {
        return call_port_status(buf, index, arity, fd_out);
    }


    if (check_name(fname, "foo", arity, 2)) {
        return call_foo(buf, index, arity, fd_out);
    }
    if (check_name(fname, "bar", arity, 2)) {
        return call_bar(buf, index, arity, fd_out);
    }

    fail(fname, fd_out);
    return 0;
}

int call_config_info(const char *buf, int *index, int arity, int fd_out){
    PRE
    CHECK_AEROSPIKE_INIT     
    OK0

    as_config  *config = &as.config;   
    as_vector  *hosts = config->hosts;

    ei_x_encode_map_header(&res_buf, 29);
    ei_x_encode_string(&res_buf, "user");
    ei_x_encode_string(&res_buf, config->user);
    ei_x_encode_string(&res_buf, "password");
    ei_x_encode_string(&res_buf, config->password);
    ei_x_encode_string(&res_buf, "number_of_hosts");
    ei_x_encode_ulong(&res_buf, hosts == NULL ? 0 : hosts->size);
    ei_x_encode_string(&res_buf, "cluster_name");
    ei_x_encode_string(&res_buf, config->cluster_name == NULL ? "" : config->cluster_name);
    ei_x_encode_string(&res_buf, "ip_map_size");
    ei_x_encode_ulong(&res_buf, config->ip_map_size);
    ei_x_encode_string(&res_buf, "min_conns_per_node");
    ei_x_encode_ulong(&res_buf, config->min_conns_per_node);
    ei_x_encode_string(&res_buf, "max_conns_per_node");
    ei_x_encode_ulong(&res_buf, config->max_conns_per_node);
    ei_x_encode_string(&res_buf, "async_min_conns_per_node");
    ei_x_encode_ulong(&res_buf, config->async_min_conns_per_node);
    ei_x_encode_string(&res_buf, "async_max_conns_per_node");
    ei_x_encode_ulong(&res_buf, config->async_max_conns_per_node);
    ei_x_encode_string(&res_buf, "pipe_max_conns_per_node");
    ei_x_encode_ulong(&res_buf, config->pipe_max_conns_per_node);
    ei_x_encode_string(&res_buf, "conn_pools_per_node");
    ei_x_encode_ulong(&res_buf, config->conn_pools_per_node);
    ei_x_encode_string(&res_buf, "conn_timeout_ms");
    ei_x_encode_ulong(&res_buf, config->conn_timeout_ms);
    ei_x_encode_string(&res_buf, "login_timeout_ms");
    ei_x_encode_ulong(&res_buf, config->login_timeout_ms);
    ei_x_encode_string(&res_buf, "max_socket_idle");
    ei_x_encode_ulong(&res_buf, config->max_socket_idle);
    ei_x_encode_string(&res_buf, "max_error_rate");
    ei_x_encode_ulong(&res_buf, config->max_error_rate);
    ei_x_encode_string(&res_buf, "error_rate_window");
    ei_x_encode_ulong(&res_buf, config->error_rate_window);
    ei_x_encode_string(&res_buf, "tender_interval");
    ei_x_encode_ulong(&res_buf, config->tender_interval);
    ei_x_encode_string(&res_buf, "thread_pool_size");
    ei_x_encode_ulong(&res_buf, config->thread_pool_size);
    ei_x_encode_string(&res_buf, "tend_thread_cpu");
    ei_x_encode_long(&res_buf, config->tend_thread_cpu);
    ei_x_encode_string(&res_buf, "fail_if_not_connected");
    ei_x_encode_boolean(&res_buf, config->fail_if_not_connected);
    ei_x_encode_string(&res_buf, "use_services_alternate");
    ei_x_encode_boolean(&res_buf, config->use_services_alternate);
    ei_x_encode_string(&res_buf, "rack_aware");
    ei_x_encode_boolean(&res_buf, config->rack_aware);
    ei_x_encode_string(&res_buf, "rack_id");
    ei_x_encode_long(&res_buf, config->rack_id);
    ei_x_encode_string(&res_buf, "use_shm");
    ei_x_encode_boolean(&res_buf, config->use_shm);
    ei_x_encode_string(&res_buf, "shm_key");
    ei_x_encode_long(&res_buf, config->shm_key);
    ei_x_encode_string(&res_buf, "shm_max_nodes");
    ei_x_encode_ulong(&res_buf, config->shm_max_nodes);
    ei_x_encode_string(&res_buf, "shm_max_namespaces");
    ei_x_encode_ulong(&res_buf, config->shm_max_namespaces);
    ei_x_encode_string(&res_buf, "shm_takeover_threshold_sec");
    ei_x_encode_ulong(&res_buf, config->shm_takeover_threshold_sec);
// --------------------
    ei_x_encode_string(&res_buf, "policy_info");
    ei_x_encode_map_header(&res_buf, 3);
    ei_x_encode_string(&res_buf, "timeout");
    ei_x_encode_ulong(&res_buf, config->policies.info.timeout);
    ei_x_encode_string(&res_buf, "send_as_is");
    ei_x_encode_boolean(&res_buf, config->policies.info.send_as_is);
    ei_x_encode_string(&res_buf, "check_bounds");
    ei_x_encode_boolean(&res_buf, config->policies.info.check_bounds);


    end:
    POST
}

int call_cluster_info(const char *buf, int *index, int arity, int fd_out){
    PRE
    CHECK_ALL
    OK0

    as_cluster *cluster = as.cluster;

    ei_x_encode_map_header(&res_buf, 6);
    ei_x_encode_string(&res_buf, "cluster_name");
    ei_x_encode_string(&res_buf, cluster->cluster_name == NULL ? "" : cluster->cluster_name);
    ei_x_encode_string(&res_buf, "user");
    ei_x_encode_string(&res_buf, cluster->user == NULL ? "" : cluster->user);
    ei_x_encode_string(&res_buf, "password");
    ei_x_encode_string(&res_buf, cluster->password == NULL ? "" : cluster->password);
    ei_x_encode_string(&res_buf, "nodes_number");
    ei_x_encode_long(&res_buf, cluster->nodes == NULL ? 0 : cluster->nodes->size);
    ei_x_encode_string(&res_buf, "random_node_index");
    ei_x_encode_long(&res_buf, cluster->node_index);
    ei_x_encode_string(&res_buf, "n_partitions");
    ei_x_encode_long(&res_buf, cluster->n_partitions);

    end:
    POST
}

int call_port_status(const char *buf, int *index, int arity, int fd_out) {
    PRE
    OK0

    ei_x_encode_map_header(&res_buf, 2);
    ei_x_encode_string(&res_buf, "is_aerospike_initialised");
    ei_x_encode_boolean(&res_buf, is_aerospike_initialised);
    ei_x_encode_string(&res_buf, "is_connected");
    ei_x_encode_boolean(&res_buf, is_connected);

    POST
}

int call_aerospike_init(const char *buf, int *index, int arity, int fd_out) {
    PRE
    if (!is_aerospike_initialised) {
        as_config config;
        as_config_init(&config);
        aerospike_init(&as, &config);
        is_aerospike_initialised = 1;
    } 
    OK("initialised")
    // end:
    POST
}

int call_config_add_hosts(const char *buf, int *index, int arity, int fd_out) {
    PRE
    char host[MAX_HOST_SIZE];
    long port;

    if (ei_decode_string(buf, index, host) != 0) {
        ERROR("invalid first argument: host")
        goto end;
    } 
    if (ei_decode_long(buf, index, &port) != 0) {
        ERROR("invalid second argument: port")
        goto end;
    }
    CHECK_INIT

	if (! as_config_add_hosts(&as.config, host, port)) {
        ERROR("failed to add host or port")
		// as_event_close_loops(); ???
        goto end;
	}

    OK("host and port added")

    end:
    POST
}

int call_connect(const char *buf, int *index, int arity, int fd_out) {
    PRE
    char user[AS_USER_SIZE];
    char password[AS_PASSWORD_SIZE];

    if (ei_decode_string(buf, index, user) != 0) {
        ERROR("invalid first argument: user name")
        goto end;
    } 
    if (ei_decode_string(buf, index, password) != 0) {
        ERROR("invalid seconf argument: password")
        goto end;
    } 

    CHECK_INIT

    as_config_set_user(&as.config, user, password);
	as_error err;
 
	if (aerospike_connect(&as, &err) != AEROSPIKE_OK) {
		// as_event_close_loops();
        ERROR(err.message)
        is_connected = 0;
        goto end;
	}

    is_connected = 1;
    OK("connected")

    end:
    POST
}

int call_key_put(const char *buf, int *index, int arity, int fd_out) {
    PRE

    char bin[AS_BIN_NAME_MAX_SIZE];
    long val;
    char namespace[MAX_NAMESPACE_SIZE];
    char set[MAX_SET_SIZE];
    char key_str[MAX_KEY_STR_SIZE];

    if (ei_decode_string(buf, index, namespace) != 0) {
        ERROR("invalid first argument: namespace")
        goto end;
    } 
    if (ei_decode_string(buf, index, set) != 0) {
        ERROR("invalid second argument: set")
        goto end;
    } 
    if (ei_decode_string(buf, index, key_str) != 0) {
        ERROR("invalid third argument: key_str")
        goto end;
    } 
    if (ei_decode_string(buf, index, bin) != 0) {
        ERROR("invalid fourth argument: bin")
        goto end;
    }
    if (ei_decode_long(buf, index, &val) != 0) {
        ERROR("invalid fifth argument: val")
        goto end;
    }

    CHECK_ALL

    as_key key;
	as_key_init_str(&key, namespace, set, key_str);

	as_record rec;
	as_record_inita(&rec, 1);
	as_record_set_int64(&rec, bin, val);
    
	as_error err;
    // Write the record to the database.
	if (aerospike_key_put(&as, &err, NULL, &key, &rec) != AEROSPIKE_OK) {
        ERROR(err.message)
        goto end;
	}

    OK("key_put")

    end:
    POST
}

int call_key_remove(const char *buf, int *index, int arity, int fd_out) {
    PRE

    char namespace[MAX_NAMESPACE_SIZE];
    char set[MAX_SET_SIZE];
    char key_str[MAX_KEY_STR_SIZE];

    if (ei_decode_string(buf, index, namespace) != 0) {
        ERROR("invalid first argument: namespace")
        goto end;
    } 
    if (ei_decode_string(buf, index, set) != 0) {
        ERROR("invalid second argument: set")
        goto end;
    } 
    if (ei_decode_string(buf, index, key_str) != 0) {
        ERROR("invalid third argument: key_str")
        goto end;
    } 

    CHECK_ALL

    as_key key;
	as_key_init_str(&key, namespace, set, key_str);

	as_error err;
    
    // Write the record to the database.
	if (aerospike_key_remove(&as, &err, NULL, &key) != AEROSPIKE_OK) {
        ERROR(err.message)
        goto end;
	}

    OK("key_remove")

    end:
    POST
}

static void dump_bin(ei_x_buff *p_res_buf, const as_bin* p_bin) {
    char* val_as_str = NULL;
    ei_x_encode_tuple_header(p_res_buf, 2);
    ei_x_encode_string(p_res_buf, as_bin_get_name(p_bin));
    switch (as_bin_get_type(p_bin)){
        case AS_INTEGER:
            ei_x_encode_long(p_res_buf, as_bin_get_value(p_bin)->integer.value);
            break;
        default:
            val_as_str = as_val_tostring(as_bin_get_value(p_bin));
            ei_x_encode_string(p_res_buf, val_as_str);
            free(val_as_str);
    }
}

static int dump_records(ei_x_buff *p_res_buf, const as_record *p_rec) {
    int res = 1;
    if (p_rec == NULL) {
        ERRORP("NULL p_rec - internal error")
        return res;
    }
	if (p_rec->key.valuep) {
		char* key_val_as_str = as_val_tostring(p_rec->key.valuep);
        OKP(key_val_as_str)
		free(key_val_as_str);
        return res;
	}

	as_record_iterator it;
	as_record_iterator_init(&it, p_rec);

    OK0P
    
	uint16_t num_bins = as_record_numbins(p_rec);
    ei_x_encode_list_header(p_res_buf, num_bins);

	while (as_record_iterator_has_next(&it)) {
		dump_bin(p_res_buf, as_record_iterator_next(&it));
	}

    ei_x_encode_empty_list(p_res_buf);
	as_record_iterator_destroy(&it);

    return res;
}

int call_key_get(const char *buf, int *index, int arity, int fd_out) {
    PRE

    char namespace[MAX_NAMESPACE_SIZE];
    char set[MAX_SET_SIZE];
    char key_str[MAX_KEY_STR_SIZE];
    as_record* p_rec = NULL;    

    if (ei_decode_string(buf, index, namespace) != 0) {
        ERROR("invalid first argument: namespace")
        goto end;
    } 
    if (ei_decode_string(buf, index, set) != 0) {
        ERROR("invalid second argument: set")
        goto end;
    } 
    if (ei_decode_string(buf, index, key_str) != 0) {
        ERROR("invalid third argument: key_str")
        goto end;
    } 

    CHECK_ALL

    as_key key;
	as_key_init_str(&key, namespace, set, key_str);
	as_error err;

    // Get the record from the database.
	if (aerospike_key_get(&as, &err, NULL, &key, &p_rec) != AEROSPIKE_OK) {
        ERROR(err.message)
        goto end;
	}

    res = dump_records(&res_buf, p_rec);

    end:
    if (p_rec != NULL) {
        as_record_destroy(p_rec);
    }

    POST
}

int call_key_generation(const char *buf, int *index, int arity, int fd_out) {
    PRE

    char namespace[MAX_NAMESPACE_SIZE];
    char set[MAX_SET_SIZE];
    char key_str[MAX_KEY_STR_SIZE];
    as_record* p_rec = NULL;    

    if (ei_decode_string(buf, index, namespace) != 0) {
        ERROR("invalid first argument: namespace")
        goto end;
    } 
    if (ei_decode_string(buf, index, set) != 0) {
        ERROR("invalid second argument: set")
        goto end;
    } 
    if (ei_decode_string(buf, index, key_str) != 0) {
        ERROR("invalid third argument: key_str")
        goto end;
    } 

    CHECK_ALL

    as_key key;
	as_key_init_str(&key, namespace, set, key_str);
	as_error err;

    // Get the record from the database.
	if (aerospike_key_get(&as, &err, NULL, &key, &p_rec) != AEROSPIKE_OK) {
        ERROR(err.message)
        goto end;
	}

    OK0
    ei_x_encode_map_header(&res_buf, 2);
    ei_x_encode_string(&res_buf, "gen");
    ei_x_encode_ulong(&res_buf, p_rec->gen);
    ei_x_encode_string(&res_buf, "ttl");
    ei_x_encode_ulong(&res_buf, p_rec->ttl);

    end:
    if (p_rec != NULL) {
        as_record_destroy(p_rec);
    }

    POST
}

int call_node_random(const char *buf, int *index, int arity, int fd_out) {
    PRE
    CHECK_ALL
    as_node* node = as_node_get_random(as.cluster);
    if (! node) {
        ERROR("Failed to find server node.");
        goto end;
	}
    OK(as_node_get_address_string(node))
    as_node_release(node);

    end:
    POST
}

int call_node_names(const char *buf, int *index, int arity, int fd_out) {
    PRE
    CHECK_ALL
    int n_nodes = 0;
    char* node_names = 0;
    as_cluster_get_node_names(as.cluster, &n_nodes, &node_names);

    OK0
    ei_x_encode_list_header(&res_buf, n_nodes);
    for(int i = 0; i < n_nodes; i++){
        ei_x_encode_string(&res_buf, &node_names[i]);
    }
    ei_x_encode_empty_list(&res_buf);

    end:
    POST
}

int call_node_get(const char *buf, int *index, int arity, int fd_out) {
    PRE
    char node_name[AS_NODE_NAME_MAX_SIZE];
    if (ei_decode_string(buf, index, node_name) != 0) {
        ERROR("invalid first argument: node_name")
        goto end;
    }
    CHECK_ALL

    as_node* node = as_node_get_by_name(as.cluster, node_name);
    if (! node) {
        ERROR("Failed to find server node.");
        goto end;
	}
    OK(as_node_get_address_string(node))
    as_node_release(node);

    end:
    POST
}

int call_node_info(const char *buf, int *index, int arity, int fd_out) {
    PRE
    char node_name[AS_NODE_NAME_MAX_SIZE];
    char item[1024];
    as_node* node = NULL;

    if (ei_decode_string(buf, index, node_name) != 0) {
        ERROR("invalid first argument: node_name")
        goto end;
    }
    if (ei_decode_string(buf, index, item) != 0) {
        ERROR("invalid second argument: item")
        goto end;
    }
    CHECK_ALL

	as_cluster* cluster = as.cluster;
    node = as_node_get_by_name(cluster, node_name);
    if (! node) {
        ERROR("Failed to find server node.");
        goto end;
	}

    char *info = NULL;
    as_error err;
    const as_policy_info* policy = &as.config.policies.info;
	uint64_t deadline = as_socket_deadline(policy->timeout);
    as_status status = AEROSPIKE_ERR_CLUSTER;

    status = as_info_command_node(&err, node, (char*)item, policy->send_as_is, deadline, &info);
    if (status != AEROSPIKE_OK) {
        ERROR(err.in_doubt == true ? "unknown error" : err.message)
        goto end;
    }

    if (info == NULL) {
        ERROR("no data")
        goto end;
    }

    OK0
    ei_x_encode_list_header(&res_buf, 1);
    ei_x_encode_string(&res_buf, &info[0]);
    ei_x_encode_empty_list(&res_buf);
    free(info);

    end:
    as_node_release(node);
    POST
}

int call_host_info(const char *buf, int *index, int arity, int fd_out) {
    PRE
    char hostname[AS_NODE_NAME_MAX_SIZE];
    char item[1024];
    long port;
    if (ei_decode_string(buf, index, hostname) != 0) {
        ERROR("invalid first argument: hostname")
        goto end;
    }
    if (ei_decode_long(buf, index, &port) != 0) {
        ERROR("invalid second argument: port")
        goto end;
    }
    if (ei_decode_string(buf, index, item) != 0) {
        ERROR("invalid third argument: item")
        goto end;
    }
    CHECK_ALL

    as_error err;
    as_address_iterator iter;

	as_status status = as_lookup_host(&iter, &err, hostname, port);
	
	if (status) {
        ERROR(err.in_doubt == true ? "unknown error" : err.message)
        goto end;
	}
    
	as_cluster* cluster = as.cluster;
    char *info = NULL;
    const as_policy_info* policy = &as.config.policies.info;
	uint64_t deadline = as_socket_deadline(policy->timeout);
	struct sockaddr* addr;
	bool loop = true;

	while (loop && as_lookup_next(&iter, &addr)) {
		status = as_info_command_host(cluster, &err, addr, (char*)item, policy->send_as_is, deadline, &info, hostname);
		
		switch (status) {
			case AEROSPIKE_OK:
			case AEROSPIKE_ERR_TIMEOUT:
			case AEROSPIKE_ERR_INDEX_FOUND:
			case AEROSPIKE_ERR_INDEX_NOT_FOUND:
				loop = false;
				break;
				
			default:
				break;
		}
	}
	as_lookup_end(&iter);

    if(info == NULL){
        ERROR("no data")
        goto end;
    }

    OK0
    ei_x_encode_list_header(&res_buf, 1);
    ei_x_encode_string(&res_buf, &info[0]);
    ei_x_encode_empty_list(&res_buf);
    free(&info[0]);


    end:
    POST
}

int call_help(const char *buf, int *index, int arity, int fd_out) {
    PRE
    char item[1024];
    if (ei_decode_string(buf, index, item) != 0) {
        ERROR("invalid first argument: item")
        goto end;
    } 

    CHECK_ALL  

	as_error err;
	as_status rc = AEROSPIKE_OK;
    char * info = NULL;

    rc = aerospike_info_any(&as, &err, NULL, item, &info);
	if (rc != AEROSPIKE_OK) {
        ERROR(err.in_doubt == true ? "unknown error" : err.message)
        goto end;
	}

    if(info == NULL){
        ERROR("no data")
        goto end;
    }

    OK(info)
    free(info);

    end:
    POST
}
