/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;


static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                         void *set_extra),
                      void *set_extra) {
    set_val(extra, set_extra);
    return 0;
}


static int generate_row_for_del(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT *dest_key,
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    )
{
    dest_key->size=0;
    return 0;
}

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT *dest_key, 
    DBT *dest_val,
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    ) 
{
    uint8_t src_val_data;
    assert(src_val->size == 1);
    src_val_data = *(uint8_t *)src_val->data;
    assert(src_val_data == 100);
    dest_key->size=0;
    dest_val->size=0;
    return 0;
}

static void setup (void) {
    { int chk_r = system("rm -rf " ENVDIR); CKERR(chk_r); }
    { int chk_r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    { int chk_r = env->set_generate_row_callback_for_put(env,generate_row_for_put); CKERR(chk_r); }
    { int chk_r = env->set_generate_row_callback_for_del(env,generate_row_for_del); CKERR(chk_r); }
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static void run_test(void) {
    DB* db = NULL;
    DB_LOADER* loader = NULL;
    DBT key, val;
    uint32_t mult_db_flags = 0;
    uint32_t mult_dbt_flags = DB_DBT_REALLOC;
    uint8_t key_data = 0;
    uint8_t val_data = 0;
    

    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            { int chk_r = db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
        });


    dbt_init(&key,&key_data,sizeof(uint8_t));
    dbt_init(&val,&val_data,sizeof(uint8_t));

    val_data = 1;


    //
    // now do an update broadcast that will set the val to something bigger
    //
    val_data = 100;
    IN_TXN_COMMIT(env, NULL, txn_broadcast, 0, {
            { int chk_r = db->update_broadcast(db, txn_broadcast, &val, DB_IS_RESETTING_OP); CKERR(chk_r); }
        });

    //
    // now create a loader
    //
    IN_TXN_COMMIT(env, NULL, txn_loader, 0, {
        // create DB
            { int chk_r = env->create_loader(
            env,
            txn_loader,
            &loader,
            db,
            1,
            &db,
            &mult_db_flags,
            &mult_dbt_flags,
            0
                    ); CKERR(chk_r); }
            { int chk_r = loader->put(loader, &key, &val); CKERR(chk_r); }
            { int chk_r = loader->close(loader); CKERR(chk_r); }
        });

    IN_TXN_COMMIT(env, NULL, txn_update, 0, {
            { int chk_r = db->update(db, txn_update, &key, &val, 0); CKERR(chk_r); }
        });

    { int chk_r = db->close(db, 0); CKERR(chk_r); }

}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}