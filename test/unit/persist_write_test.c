/* Tests for persistence.
 *
 * FIXME - these need to be aggressive about finding failures, at the moment
 * they are just confirming that good behaviour works. */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#define WITH_BROKER
#define WITH_PERSISTENCE

#include "mosquitto_broker_internal.h"
#include "persist.h"

uint64_t last_retained;
char *last_sub = NULL;
int last_qos;


/* read entire file into memory */
static int file_read(const char *filename, uint8_t **data, size_t *len)
{
	FILE *fptr;
	size_t rc;

	fptr = fopen(filename, "rb");
	if(!fptr) return 1;

	fseek(fptr, 0, SEEK_END);
	*len = ftell(fptr);
	*data = malloc(*len);
	if(!(*data)){
		fclose(fptr);
		return 1;
	}
	fseek(fptr, 0, SEEK_SET);
	rc = fread(*data, 1, *len, fptr);
	fclose(fptr);

	if(rc == *len){
		return 0;
	}else{
		*len = 0;
		free(*data);
		return 1;
	}
}

/* Crude file diff, only for small files */
static int file_diff(const char *one, const char *two)
{
	size_t len1, len2;
	uint8_t *data1 = NULL, *data2 = NULL;
	int rc = 1;

	if(file_read(one, &data1, &len1)){
		return 1;
	}

	if(file_read(two, &data2, &len2)){
		free(data1);
		return 1;
	}

	if(len1 == len2){
		rc = memcmp(data1, data2, len1);
	}
	free(data1);
	free(data2);

	return rc;
}

static void TEST_persistence_disabled(void)
{
	struct mosquitto_db db;
	struct mosquitto__config config;
	int rc;

	memset(&db, 0, sizeof(struct mosquitto_db));
	memset(&config, 0, sizeof(struct mosquitto__config));
	db.config = &config;

	rc = persist__backup(&db, false);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_INVAL);

	config.persistence_filepath = "disabled.db";
	rc = persist__backup(&db, false);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);
}


static void TEST_empty_file(void)
{
	struct mosquitto_db db;
	struct mosquitto__config config;
	int rc;

	memset(&db, 0, sizeof(struct mosquitto_db));
	memset(&config, 0, sizeof(struct mosquitto__config));
	db.config = &config;

	config.persistence = true;

	config.persistence_filepath = "empty.db";
	rc = persist__backup(&db, false);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);
	CU_ASSERT_EQUAL(0, file_diff("files/persist_write/empty.test-db", "empty.db"));
	unlink("empty.db");
}


static void TEST_v4_config_ok(void)
{
	struct mosquitto_db db;
	struct mosquitto__config config;
	int rc;

	memset(&db, 0, sizeof(struct mosquitto_db));
	memset(&config, 0, sizeof(struct mosquitto__config));
	db.config = &config;

	config.persistence = true;
	config.persistence_filepath = "files/persist_read/v4-cfg.test-db";
	rc = persist__restore(&db);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);

	config.persistence_filepath = "v4-cfg.db";
	rc = persist__backup(&db, true);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);

	CU_ASSERT_EQUAL(0, file_diff("files/persist_read/v4-cfg.test-db", "v4-cfg.db"));
	unlink("v4-cfg.db");
}


static void TEST_v4_message_store_no_ref(void)
{
	struct mosquitto_db db;
	struct mosquitto__config config;
	int rc;

	memset(&db, 0, sizeof(struct mosquitto_db));
	memset(&config, 0, sizeof(struct mosquitto__config));
	db.config = &config;

	config.persistence = true;
	config.persistence_filepath = "files/persist_read/v4-message-store.test-db";
	rc = persist__restore(&db);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);

	config.persistence_filepath = "v4-message-store-no-ref.db";
	rc = persist__backup(&db, true);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);

	CU_ASSERT_EQUAL(0, file_diff("files/persist_write/v4-message-store-no-ref.test-db", "v4-message-store-no-ref.db"));
	unlink("v4-message-store-no-ref.db");
}


#if 0
NOT WORKING
static void TEST_v4_full(void)
{
	struct mosquitto_db db;
	struct mosquitto__config config;
	int rc;

	memset(&db, 0, sizeof(struct mosquitto_db));
	memset(&config, 0, sizeof(struct mosquitto__config));
	db.config = &config;

	db__open(&config, &db);

	config.persistence = true;
	config.persistence_filepath = "files/persist_write/v4-full.test-db";
	rc = persist__restore(&db);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);

	config.persistence_filepath = "v4-full.db";
	rc = persist__backup(&db, true);
	CU_ASSERT_EQUAL(rc, MOSQ_ERR_SUCCESS);

	CU_ASSERT_EQUAL(0, file_diff("files/persist_write/v4-full.test-db", "v4-full.db"));
	unlink("v4-full.db");
}
#endif


/* ========================================================================
 * TEST SUITE SETUP
 * ======================================================================== */


int main(int argc, char *argv[])
{
	CU_pSuite test_suite = NULL;
	unsigned int fails;

    if(CU_initialize_registry() != CUE_SUCCESS){
        printf("Error initializing CUnit registry.\n");
        return 1;
    }

	test_suite = CU_add_suite("Persist write", NULL, NULL);
	if(!test_suite){
		printf("Error adding CUnit persist write test suite.\n");
        CU_cleanup_registry();
		return 1;
	}

	if(0
			|| !CU_add_test(test_suite, "Persistence disabled", TEST_persistence_disabled)
			|| !CU_add_test(test_suite, "Empty file", TEST_empty_file)
			|| !CU_add_test(test_suite, "v4 config ok", TEST_v4_config_ok)
			|| !CU_add_test(test_suite, "v4 message store (message has no refs)", TEST_v4_message_store_no_ref)
			//|| !CU_add_test(test_suite, "v4 full", TEST_v4_full)
			){

		printf("Error adding persist CUnit tests.\n");
		CU_cleanup_registry();
        return 1;
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
	fails = CU_get_number_of_failures();
    CU_cleanup_registry();

    return (int)fails;
}