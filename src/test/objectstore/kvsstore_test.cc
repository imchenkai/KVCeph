#include "kvsstore_test_impl.h"

/// --------------------------------------------------------------------
/// Unittest starts
/// --------------------------------------------------------------------


TEST_P(KvsStoreTest, SimpleRemount2) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    ghobject_t hoid2(hobject_t(sobject_t("Object 2", CEPH_NOSNAP)));
    bufferlist bl;
    bl.append("1234512345");
    int r;
    {
        cerr << "create collection + write" << std::endl;
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.write(cid, hoid, 0, bl.length(), bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid2, 0, bl.length(), bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        bool exists = store->exists(cid, hoid);
        ASSERT_TRUE(!exists);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, SimpleObjectTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        bufferlist in;
        r = store->read(cid, hoid, 0, 5, in);
        ASSERT_EQ(-ENOENT, r);
    }
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {

        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        cerr << "Creating object " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.touch(cid, hoid);
        cerr << "Remove then create" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        bufferlist bl, orig;
        bl.append("abcde");
        orig = bl;
        t.remove(cid, hoid);
        t.write(cid, hoid, 0, 5, bl);
        cerr << "Remove then create" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in;
        r = store->read(cid, hoid, 0, 5, in);
        ASSERT_EQ(5, r);
        ASSERT_TRUE(bl_eq(orig, in));
    }
    {
        ObjectStore::Transaction t;
        bufferlist bl, exp;
        bl.append("abcde");
        exp = bl;
        exp.append(bl);
        t.write(cid, hoid, 5, 5, bl);
        cerr << "Append" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in;
        r = store->read(cid, hoid, 0, 10, in);
        ASSERT_EQ(10, r);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        bufferlist bl, exp;
        bl.append("abcdeabcde");
        exp = bl;
        t.write(cid, hoid, 0, 10, bl);
        cerr << "Full overwrite" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in;
        r = store->read(cid, hoid, 0, 10, in);
        ASSERT_EQ(10, r);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        bufferlist bl;
        bl.append("abcde");
        t.write(cid, hoid, 3, 5, bl);
        cerr << "Partial overwrite" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in, exp;
        exp.append("abcabcdede");
        r = store->read(cid, hoid, 0, 10, in);
        ASSERT_EQ(10, r);
        in.hexdump(cout);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        {
            ObjectStore::Transaction t;
            bufferlist bl;
            bl.append("fghij");
            t.truncate(cid, hoid, 0);
            t.write(cid, hoid, 5, 5, bl);
            cerr << "Truncate + hole" << std::endl;
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
        {
            ObjectStore::Transaction t;
            bufferlist bl;
            bl.append("abcde");
            t.write(cid, hoid, 0, 5, bl);
            cerr << "Reverse fill-in" << std::endl;
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }

        bufferlist in, exp;
        exp.append("abcdefghij");
        r = store->read(cid, hoid, 0, 10, in);
        ASSERT_EQ(10, r);
        in.hexdump(cout);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        bufferlist bl;
        bl.append("abcde01234012340123401234abcde01234012340123401234abcde01234012340123401234abcde01234012340123401234");
        t.write(cid, hoid, 0, bl.length(), bl);
        cerr << "larger overwrite" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in;
        r = store->read(cid, hoid, 0, bl.length(), in);
        ASSERT_EQ((int)bl.length(), r);
        in.hexdump(cout);
        ASSERT_TRUE(bl_eq(bl, in));
    }
    {
        bufferlist bl;
        bl.append("abcde01234012340123401234abcde01234012340123401234abcde01234012340123401234abcde01234012340123401234");

        bufferlist in;
        r = store->read(cid, hoid, 0, 0, in);
        ASSERT_EQ((int)bl.length(), r);
        in.hexdump(cout);
        ASSERT_TRUE(bl_eq(bl, in));
    }
    {
        //verifying unaligned csums
        std::string s1("1"), s2(0x1000, '2'), s3("00");
        {
            ObjectStore::Transaction t;
            bufferlist bl;
            bl.append(s1);
            bl.append(s2);
            t.truncate(cid, hoid, 0);
            t.write(cid, hoid, 0x1000-1, bl.length(), bl);
            cerr << "Write unaligned csum, stage 1" << std::endl;
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }

        bufferlist in, exp1, exp2, exp3;
        exp1.append(s1);
        exp2.append(s2);
        exp3.append(s3);
        r = store->read(cid, hoid, 0x1000-1, 1, in);
        ASSERT_EQ(1, r);
        ASSERT_TRUE(bl_eq(exp1, in));
        in.clear();
        r = store->read(cid, hoid, 0x1000, 0x1000, in);
        ASSERT_EQ(0x1000, r);
        ASSERT_TRUE(bl_eq(exp2, in));

        {
            ObjectStore::Transaction t;
            bufferlist bl;
            bl.append(s3);
            t.write(cid, hoid, 1, bl.length(), bl);
            cerr << "Write unaligned csum, stage 2" << std::endl;
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
        in.clear();
        r = store->read(cid, hoid, 1, 2, in);
        ASSERT_EQ(2, r);
        ASSERT_TRUE(bl_eq(exp3, in));
        in.clear();
        r = store->read(cid, hoid, 0x1000-1, 1, in);
        ASSERT_EQ(1, r);
        ASSERT_TRUE(bl_eq(exp1, in));
        in.clear();
        r = store->read(cid, hoid, 0x1000, 0x1000, in);
        ASSERT_EQ(0x1000, r);
        ASSERT_TRUE(bl_eq(exp2, in));

    }
    derr << "cleaning..." << dendl;
    {
        ObjectStore::Transaction t;
        derr << "deleting " << hoid << dendl;
        t.remove(cid, hoid);
        //t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    set<ghobject_t> listed, listed2;
    vector<ghobject_t> objects;


    ASSERT_EQ(r, 0);

    for (vector<ghobject_t> ::iterator i = objects.begin();
         i != objects.end();
         ++i) {
            derr << "extra: " << *i << dendl;
    }

    derr << "cleaning..." << dendl;
    {
        ObjectStore::Transaction t;
        derr << "deleting " << hoid << dendl;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, ManyObjectTest) {

    ObjectStore::Sequencer osr("test");
    int NUM_OBJS = 2000;
    int r = 0;
    coll_t cid;
    string base = "";
    for (int i = 0; i < 20; ++i) base.append("aaaaa");
    set<ghobject_t> created;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    { // MAKE SURE THE COLLECTION IS EMPTY
        set<ghobject_t> listed, listed2;
        vector<ghobject_t> objects;
        int r = store->collection_list(cid, ghobject_t(), ghobject_t::get_max(), INT_MAX, &objects, 0);
        ASSERT_EQ(r, 0);

        for (vector<ghobject_t> ::iterator i = objects.begin();
             i != objects.end();
             ++i) {
            derr << "extra: " << *i << dendl;
        }
        ASSERT_EQ((int)objects.size(), 0);
    }
    for (int i = 0; i < NUM_OBJS; ++i) {
        if (!(i % 5)) {
            //cerr << "Object " << i << std::endl;
        }
        ObjectStore::Transaction t;
        char buf[100];
        snprintf(buf, sizeof(buf), "%d", i);
        ghobject_t hoid(hobject_t(sobject_t(string(buf) + base, CEPH_NOSNAP)));
        t.touch(cid, hoid);
        created.insert(hoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    for (set<ghobject_t>::iterator i = created.begin();
         i != created.end();
         ++i) {
        struct stat buf;
        ASSERT_TRUE(!store->stat(cid, *i, &buf));
    }
    set<ghobject_t> listed, listed2;
    vector<ghobject_t> objects;
    r = store->collection_list(cid, ghobject_t(), ghobject_t::get_max(), INT_MAX, &objects, 0);
    ASSERT_EQ(r, 0);
    for (vector<ghobject_t> ::iterator i = objects.begin();
         i != objects.end();
         ++i) {
        listed.insert(*i);
        if (!created.count(*i)) {
            derr << "extra: " << *i << dendl;
        }
    }
    ASSERT_EQ(objects.size(),created.size());
    for (vector<ghobject_t> ::iterator i = objects.begin();
         i != objects.end();
         ++i) {
        listed.insert(*i);
        ASSERT_TRUE(created.count(*i));
    }

    ghobject_t start, next;
    objects.clear();
    r = store->collection_list(
            cid,
            ghobject_t::get_max(),
            ghobject_t::get_max(),
            50,
            &objects,
            &next
    );
    ASSERT_EQ(r, 0);
    ASSERT_TRUE(objects.empty());
    objects.clear();
    listed.clear();
    ghobject_t start2, next2;
    while (1) {
        r = store->collection_list(cid, start, ghobject_t::get_max(),
                                   50,
                                   &objects,
                                   &next);
        ASSERT_TRUE(sorted(objects));
        ASSERT_EQ(r, 0);
        listed.insert(objects.begin(), objects.end());
        if (objects.size() < 50) {
            ASSERT_TRUE(next.is_max());
            break;
        }
        objects.clear();

        start = next;
    }
    ASSERT_TRUE(listed.size() == created.size());
    if (listed2.size()) {
        ASSERT_EQ(listed.size(), listed2.size());
    }

    for (set<ghobject_t>::iterator i = listed.begin();
         i != listed.end();
         ++i) {
        ASSERT_TRUE(created.count(*i));
    }

    for (set<ghobject_t>::iterator i = created.begin();
         i != created.end();
         ++i) {
        ObjectStore::Transaction t;
        t.remove(cid, *i);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    cerr << "cleaning up" << std::endl;
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


// Individual testing
TEST_P(KvsStoreTest, OmapCloneTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP),
                              "key", 123, -1, ""));
    bufferlist small;
    small.append("small");
    map<string,bufferlist> km;
    km["foo"] = small;
    km["bar"].append("asdfjkasdkjdfsjkafskjsfdj");
    bufferlist header;
    header.append("this is a header");
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        t.omap_setkeys(cid, hoid, km);
        t.omap_setheader(cid, hoid, header);
        cerr << "Creating object and set omap " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid2(hobject_t(sobject_t("Object 2", CEPH_NOSNAP),
                               "key", 123, -1, ""));
    {
        ObjectStore::Transaction t;
        t.clone(cid, hoid, hoid2);
        cerr << "Clone object" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        map<string,bufferlist> r;
        bufferlist h;
        store->omap_get(cid, hoid2, &h, &r);
        ASSERT_TRUE(bl_eq(header, h));
        ASSERT_EQ(r.size(), km.size());
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, OmapSimple) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid(hobject_t(sobject_t("omap_obj", CEPH_NOSNAP),
                              "key", 123, -1, ""));
    bufferlist small;
    small.append("small");
    map<string,bufferlist> km;
    km["foo"] = small;
    km["bar"].append("asdfjkasdkjdfsjkafskjsfdj");
    bufferlist header;
    header.append("this is a header");
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        t.omap_setkeys(cid, hoid, km);
        t.omap_setheader(cid, hoid, header);
        cerr << "Creating object and set omap " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    // get header, keys
    {
        bufferlist h;
        map<string,bufferlist> r;
        store->omap_get(cid, hoid, &h, &r);
        ASSERT_TRUE(bl_eq(header, h));
        ASSERT_EQ(r.size(), km.size());
        cout << "r: " << r << std::endl;
    }
    // test iterator with seek_to_first
    {
        map<string,bufferlist> r;
        ObjectMap::ObjectMapIterator iter = store->get_omap_iterator(cid, hoid);
        for (iter->seek_to_first(); iter->valid(); iter->next(false)) {
            derr << "found key = " << iter->key() << dendl;
            r[iter->key()] = iter->value();
        }
        cout << "r: " << r << std::endl;
        ASSERT_EQ(r.size(), km.size());
    }
    // test iterator with initial lower_bound
    {
        map<string,bufferlist> r;
        ObjectMap::ObjectMapIterator iter = store->get_omap_iterator(cid, hoid);
        for (iter->lower_bound(string()); iter->valid(); iter->next(false)) {
            r[iter->key()] = iter->value();
        }
        cout << "r: " << r << std::endl;
        ASSERT_EQ(r.size(), km.size());
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, OMapTest) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t("tesomap", "", CEPH_NOSNAP, 0, 0, ""));
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    map<string, bufferlist> attrs;
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        t.omap_clear(cid, hoid);
        map<string, bufferlist> start_set;
        t.omap_setkeys(cid, hoid, start_set);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    for (int i = 0; i < 100; i++) {
        if (!(i%5)) {
            std::cout << "On iteration " << i << std::endl;
        }
        ObjectStore::Transaction t;
        bufferlist bl;
        map<string, bufferlist> cur_attrs;
        r = store->omap_get(cid, hoid, &bl, &cur_attrs);
        ASSERT_EQ(r, 0);
        for (map<string, bufferlist>::iterator j = attrs.begin();
             j != attrs.end();
             ++j) {
            bool correct = cur_attrs.count(j->first) && string(cur_attrs[j->first].c_str()) == string(j->second.c_str());
            if (!correct) {
                std::cout << j->first << " is present in cur_attrs " << cur_attrs.count(j->first) << " times " << std::endl;
                if (cur_attrs.count(j->first) > 0) {
                    std::cout << j->second.c_str() << " : " << cur_attrs[j->first].c_str() << std::endl;
                }
            }
            ASSERT_EQ(correct, true);
        }
        ASSERT_EQ(attrs.size(), cur_attrs.size());

        char buf[100];
        snprintf(buf, sizeof(buf), "%d", i);
        bl.clear();
        bufferptr bp(buf, strlen(buf) + 1);
        bl.append(bp);
        map<string, bufferlist> to_add;
        to_add.insert(pair<string, bufferlist>("key-" + string(buf), bl));
        attrs.insert(pair<string, bufferlist>("key-" + string(buf), bl));
        t.omap_setkeys(cid, hoid, to_add);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    int i = 0;
    while (attrs.size()) {
        if (!(i%5)) {
            std::cout << "removal: On iteration " << i << std::endl;
        }
        ObjectStore::Transaction t;
        bufferlist bl;
        map<string, bufferlist> cur_attrs;
        r = store->omap_get(cid, hoid, &bl, &cur_attrs);
        ASSERT_EQ(r, 0);
        for (map<string, bufferlist>::iterator j = attrs.begin();
             j != attrs.end();
             ++j) {
            bool correct = cur_attrs.count(j->first) && string(cur_attrs[j->first].c_str()) == string(j->second.c_str());
            if (!correct) {
                std::cout << j->first << " is present in cur_attrs " << cur_attrs.count(j->first) << " times " << std::endl;
                if (cur_attrs.count(j->first) > 0) {
                    std::cout << j->second.c_str() << " : " << cur_attrs[j->first].c_str() << std::endl;
                }
            }
            ASSERT_EQ(correct, true);
        }

        string to_remove = attrs.begin()->first;
        set<string> keys_to_remove;
        keys_to_remove.insert(to_remove);
        t.omap_rmkeys(cid, hoid, keys_to_remove);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        attrs.erase(to_remove);

        ++i;
    }

    {
        bufferlist bl1;
        bl1.append("omap_header");
        ObjectStore::Transaction t;
        t.omap_setheader(cid, hoid, bl1);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        t = ObjectStore::Transaction();

        bufferlist bl2;
        bl2.append("value");
        map<string, bufferlist> to_add;
        to_add.insert(pair<string, bufferlist>("key", bl2));
        t.omap_setkeys(cid, hoid, to_add);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist bl3;
        map<string, bufferlist> cur_attrs;
        r = store->omap_get(cid, hoid, &bl3, &cur_attrs);
        ASSERT_EQ(r, 0);
        ASSERT_EQ(cur_attrs.size(), size_t(1));
        ASSERT_TRUE(bl_eq(bl1, bl3));

        set<string> keys;
        r = store->omap_get_keys(cid, hoid, &keys);
        ASSERT_EQ(r, 0);
        ASSERT_EQ(keys.size(), size_t(1));
    }

    // test omap_clear, omap_rmkey_range
    {
        {
            map<string,bufferlist> to_set;
            for (int n=0; n<10; ++n) {
                to_set[stringify(n)].append("foo");
            }
            bufferlist h;
            h.append("header");
            ObjectStore::Transaction t;
            t.remove(cid, hoid);
            t.touch(cid, hoid);
            t.omap_setheader(cid, hoid, h);
            t.omap_setkeys(cid, hoid, to_set);
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
        {
            ObjectStore::Transaction t;
            t.omap_rmkeyrange(cid, hoid, "3", "7");
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
        {
            bufferlist hdr;
            map<string,bufferlist> m;
            store->omap_get(cid, hoid, &hdr, &m);
            ASSERT_EQ(6u, hdr.length());
            ASSERT_TRUE(m.count("2"));
            ASSERT_TRUE(!m.count("3"));
            ASSERT_TRUE(!m.count("6"));
            ASSERT_TRUE(m.count("7"));
            ASSERT_TRUE(m.count("8"));
            ASSERT_EQ(6u, m.size());
        }
        {
            ObjectStore::Transaction t;
            t.omap_clear(cid, hoid);
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
        {
            bufferlist hdr;
            map<string,bufferlist> m;
            store->omap_get(cid, hoid, &hdr, &m);
            ASSERT_EQ(0u, hdr.length());
            ASSERT_EQ(0u, m.size());
        }
    }

    ObjectStore::Transaction t;
    t.remove(cid, hoid);
    t.remove_collection(cid);
    r = apply_transaction(store, &osr, std::move(t));
    ASSERT_EQ(r, 0);
}

TEST_P(KvsStoreTest, SimpleCloneRangeTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    hoid.hobj.pool = -1;
    bufferlist small, newdata;
    small.append("small");
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid, 10, 5, small);
        cerr << "Creating object and write bl " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid2(hobject_t(sobject_t("Object 2", CEPH_NOSNAP)));
    hoid2.hobj.pool = -1;
    {
        ObjectStore::Transaction t;
        t.clone_range(cid, hoid, hoid2, 10, 5, 10);
        cerr << "Clone range object" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        r = store->read(cid, hoid2, 10, 5, newdata);
        ASSERT_EQ(r, 5);
        ASSERT_TRUE(bl_eq(small, newdata));
    }
    {
        ObjectStore::Transaction t;
        t.truncate(cid, hoid, 1024*1024);
        t.clone_range(cid, hoid, hoid2, 0, 1024*1024, 0);
        cerr << "Clone range object" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        struct stat stat, stat2;
        r = store->stat(cid, hoid, &stat);
        r = store->stat(cid, hoid2, &stat2);
        ASSERT_EQ(stat.st_size, stat2.st_size);
        ASSERT_EQ(1024*1024, stat2.st_size);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, FiemapEmpty) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    int r = 0;
    ghobject_t oid(hobject_t(sobject_t("fiemap_object", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, oid);
        t.truncate(cid, oid, 100000);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bufferlist bl;
        store->fiemap(cid, oid, 0, 100000, bl);
        map<uint64_t,uint64_t> m, e;
        bufferlist::iterator p = bl.begin();
        ::decode(m, p);
        cout << " got " << m << std::endl;
        e[0] = 100000;
        EXPECT_TRUE(m == e || m.empty());
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, oid);
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, Trivial) {
}

TEST_P(KvsStoreTest, TrivialRemount) {
    int r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
}

TEST_P(KvsStoreTest, SimpleRemount) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    ghobject_t hoid2(hobject_t(sobject_t("Object 2", CEPH_NOSNAP)));
    bufferlist bl;
    bl.append("1234512345");
    int r;
    {
        cerr << "create collection + write" << std::endl;
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.write(cid, hoid, 0, bl.length(), bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid2, 0, bl.length(), bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        bool exists = store->exists(cid, hoid);
        ASSERT_TRUE(!exists);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, IORemount) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    bufferlist bl;
    bl.append("1234512345");
    int r;
    {
        cerr << "create collection + objects" << std::endl;
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        for (int n=1; n<=100; ++n) {
            ghobject_t hoid(hobject_t(sobject_t("Object " + stringify(n), CEPH_NOSNAP)));
            t.write(cid, hoid, 0, bl.length(), bl);
        }
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    // overwrites
    {
        cout << "overwrites" << std::endl;
        for (int n=1; n<=100; ++n) {
            ObjectStore::Transaction t;
            ghobject_t hoid(hobject_t(sobject_t("Object " + stringify(n), CEPH_NOSNAP)));
            t.write(cid, hoid, 1, bl.length(), bl);
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
    }
    r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
    {
        ObjectStore::Transaction t;
        for (int n=1; n<=100; ++n) {
            ghobject_t hoid(hobject_t(sobject_t("Object " + stringify(n), CEPH_NOSNAP)));
            t.remove(cid, hoid);
        }
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, UnprintableCharsName) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    string name = "funnychars_";
    for (unsigned i = 0; i < 128;++i) {
        name.push_back(i);
    }
    ghobject_t oid(hobject_t(sobject_t(name, CEPH_NOSNAP)));
    int r;
    {
        cerr << "create collection + object" << std::endl;
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, oid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    r = store->umount();
    ASSERT_EQ(0, r);
    r = store->mount();
    ASSERT_EQ(0, r);
    {
        cout << "removing" << std::endl;
        ObjectStore::Transaction t;
        t.remove(cid, oid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}



TEST_P(KvsStoreTest, SimpleMetaColTest) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    int r = 0;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "create collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "add collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SimplePGColTest) {
    ObjectStore::Sequencer osr("test");
    coll_t cid(spg_t(pg_t(1,2), shard_id_t::NO_SHARD));
    int r = 0;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 4);
        cerr << "create collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 4);
        cerr << "add collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SimpleColPreHashTest) {
    ObjectStore::Sequencer osr("test");
    // Firstly we will need to revert the value making sure
    // collection hint actually works
    int merge_threshold = g_ceph_context->_conf->filestore_merge_threshold;
    std::ostringstream oss;
    if (merge_threshold > 0) {
        oss << "-" << merge_threshold;
        g_ceph_context->_conf->set_val("filestore_merge_threshold", oss.str().c_str());
    }

    uint32_t pg_num = 128;

    boost::uniform_int<> pg_id_range(0, pg_num);
    gen_type rng(time(NULL));
    int pg_id = pg_id_range(rng);

    int objs_per_folder = abs(merge_threshold) * 16 * g_ceph_context->_conf->filestore_split_multiple;
    boost::uniform_int<> folders_range(5, 256);
    uint64_t expected_num_objs = (uint64_t)objs_per_folder * (uint64_t)folders_range(rng);

    coll_t cid(spg_t(pg_t(pg_id, 15), shard_id_t::NO_SHARD));
    int r;
    {
        // Create a collection along with a hint
        ObjectStore::Transaction t;
        t.create_collection(cid, 5);
        cerr << "create collection" << std::endl;
        bufferlist hint;
        ::encode(pg_num, hint);
        ::encode(expected_num_objs, hint);
        t.collection_hint(cid, ObjectStore::Transaction::COLL_HINT_EXPECTED_NUM_OBJECTS, hint);
        cerr << "collection hint" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        // Remove the collection
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "remove collection" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    // Revert the config change so that it does not affect the split/merge tests
    if (merge_threshold > 0) {
        oss.str("");
        oss << merge_threshold;
        g_ceph_context->_conf->set_val("filestore_merge_threshold", oss.str().c_str());
    }
}


TEST_P(KvsStoreTest, SmallBlockWrites) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("foo", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist a;
    bufferptr ap(0x1000);
    memset(ap.c_str(), 'a', 0x1000);
    a.append(ap);
    bufferlist b;
    bufferptr bp(0x1000);
    memset(bp.c_str(), 'b', 0x1000);
    b.append(bp);
    bufferlist c;
    bufferptr cp(0x1000);
    memset(cp.c_str(), 'c', 0x1000);
    c.append(cp);
    bufferptr zp(0x1000);
    zp.zero();
    bufferlist z;
    z.append(zp);
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, 0x1000, a);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in, exp;
        r = store->read(cid, hoid, 0, 0x4000, in);
        ASSERT_EQ(0x1000, r);
        exp.append(a);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0x1000, 0x1000, b);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in, exp;
        r = store->read(cid, hoid, 0, 0x4000, in);
        ASSERT_EQ(0x2000, r);
        exp.append(a);
        exp.append(b);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0x3000, 0x1000, c);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in, exp;
        r = store->read(cid, hoid, 0, 0x4000, in);
        ASSERT_EQ(0x4000, r);
        exp.append(a);
        exp.append(b);
        exp.append(z);
        exp.append(c);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0x2000, 0x1000, a);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        bufferlist in, exp;
        r = store->read(cid, hoid, 0, 0x4000, in);
        ASSERT_EQ(0x4000, r);
        exp.append(a);
        exp.append(b);
        exp.append(a);
        exp.append(c);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, 0x1000, c);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bufferlist in, exp;
        r = store->read(cid, hoid, 0, 0x4000, in);
        ASSERT_EQ(0x4000, r);
        exp.append(c);
        exp.append(b);
        exp.append(a);
        exp.append(c);
        ASSERT_TRUE(bl_eq(exp, in));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, BufferCacheReadTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        bufferlist in;
        r = store->read(cid, hoid, 0, 5, in);
        ASSERT_EQ(-ENOENT, r);
    }
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bool exists = store->exists(cid, hoid);
        ASSERT_TRUE(!exists);

        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        cerr << "Creating object " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        exists = store->exists(cid, hoid);
        ASSERT_EQ(true, exists);
    }
    {
        ObjectStore::Transaction t;
        bufferlist bl, newdata;
        bl.append("abcde");
        t.write(cid, hoid, 0, 5, bl);
        t.write(cid, hoid, 10, 5, bl);
        cerr << "TwinWrite" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        r = store->read(cid, hoid, 0, 15, newdata);
        ASSERT_EQ(r, 15);
        {
            bufferlist expected;
            expected.append(bl);
            expected.append_zero(5);
            expected.append(bl);
            ASSERT_TRUE(bl_eq(expected, newdata));
        }
    }
    //overwrite over the same extents
    {
        ObjectStore::Transaction t;
        bufferlist bl, newdata;
        bl.append("edcba");
        t.write(cid, hoid, 0, 5, bl);
        t.write(cid, hoid, 10, 5, bl);
        cerr << "TwinWrite" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        r = store->read(cid, hoid, 0, 15, newdata);
        ASSERT_EQ(r, 15);
        {
            bufferlist expected;
            expected.append(bl);
            expected.append_zero(5);
            expected.append(bl);
            ASSERT_TRUE(bl_eq(expected, newdata));
        }
    }
    //additional write to an unused region of some blob
    {
        ObjectStore::Transaction t;
        bufferlist bl2, newdata;
        bl2.append("1234567890");

        t.write(cid, hoid, 20, bl2.length(), bl2);
        cerr << "Append" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        r = store->read(cid, hoid, 0, 30, newdata);
        ASSERT_EQ(r, 30);
        {
            bufferlist expected;
            expected.append("edcba");
            expected.append_zero(5);
            expected.append("edcba");
            expected.append_zero(5);
            expected.append(bl2);

            ASSERT_TRUE(bl_eq(expected, newdata));
        }
    }
    //additional write to an unused region of some blob and partial owerite over existing extents
    {
        ObjectStore::Transaction t;
        bufferlist bl, bl2, bl3, newdata;
        bl.append("DCB");
        bl2.append("1234567890");
        bl3.append("BA");

        t.write(cid, hoid, 30, bl2.length(), bl2);
        t.write(cid, hoid, 1, bl.length(), bl);
        t.write(cid, hoid, 13, bl3.length(), bl3);
        cerr << "TripleWrite" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        r = store->read(cid, hoid, 0, 40, newdata);
        ASSERT_EQ(r, 40);
        {
            bufferlist expected;
            expected.append("eDCBa");
            expected.append_zero(5);
            expected.append("edcBA");
            expected.append_zero(5);
            expected.append(bl2);
            expected.append(bl2);

            ASSERT_TRUE(bl_eq(expected, newdata));
        }
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, ManySmallWrite) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    ghobject_t b(hobject_t(sobject_t("Object 2", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    bufferptr bp(4096);
    bp.zero();
    bl.append(bp);
    for (int i=0; i<100; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, a, i*4096, 4096, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    for (int i=0; i<100; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, b, (rand() % 512)*4096, 4096, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove(cid, b);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, MultiSmallWriteSameBlock) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    bl.append("short");
    C_SaferCond c, d;
    // touch same block in both same transaction, tls, and pipelined txns
    {
        ObjectStore::Transaction t, u;
        t.write(cid, a, 0, 5, bl, 0);
        t.write(cid, a, 5, 5, bl, 0);
        t.write(cid, a, 4094, 5, bl, 0);
        t.write(cid, a, 9000, 5, bl, 0);
        u.write(cid, a, 10, 5, bl, 0);
        u.write(cid, a, 7000, 5, bl, 0);
        vector<ObjectStore::Transaction> v = {t, u};
        store->queue_transactions(&osr, v, nullptr, &c);
    }
    {
        ObjectStore::Transaction t, u;
        t.write(cid, a, 40, 5, bl, 0);
        t.write(cid, a, 45, 5, bl, 0);
        t.write(cid, a, 4094, 5, bl, 0);
        t.write(cid, a, 6000, 5, bl, 0);
        u.write(cid, a, 610, 5, bl, 0);
        u.write(cid, a, 11000, 5, bl, 0);
        vector<ObjectStore::Transaction> v = {t, u};
        store->queue_transactions(&osr, v, nullptr, &d);
    }
    c.wait();
    d.wait();
    {
        bufferlist bl2;
        r = store->read(cid, a, 0, 16000, bl2);
        ASSERT_GE(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SmallSkipFront) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.touch(cid, a);
        t.truncate(cid, a, 3000);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bufferlist bl;
        bufferptr bp(4096);
        memset(bp.c_str(), 1, 4096);
        bl.append(bp);
        ObjectStore::Transaction t;
        t.write(cid, a, 4096, 4096, bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bufferlist bl;
        ASSERT_EQ(8192, store->read(cid, a, 0, 8192, bl));
        for (unsigned i=0; i<4096; ++i)
            ASSERT_EQ(0, bl[i]);
        for (unsigned i=4096; i<8192; ++i)
            ASSERT_EQ(1, bl[i]);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, AppendDeferredVsTailCache) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("fooo", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    unsigned min_alloc = g_conf->bluestore_min_alloc_size;
    g_conf->set_val("bluestore_inject_deferred_apply_delay", "1.0");
    g_ceph_context->_conf->apply_changes(NULL);
    unsigned size = min_alloc / 3;
    bufferptr bpa(size);
    memset(bpa.c_str(), 1, bpa.length());
    bufferlist bla;
    bla.append(bpa);
    {
        ObjectStore::Transaction t;
        t.write(cid, a, 0, bla.length(), bla, 0);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    // force cached tail to clear ...
    {
        int r = store->umount();
        ASSERT_EQ(0, r);
        r = store->mount();
        ASSERT_EQ(0, r);
    }

    bufferptr bpb(size);
    memset(bpb.c_str(), 2, bpb.length());
    bufferlist blb;
    blb.append(bpb);
    {
        ObjectStore::Transaction t;
        t.write(cid, a, bla.length(), blb.length(), blb, 0);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferptr bpc(size);
    memset(bpc.c_str(), 3, bpc.length());
    bufferlist blc;
    blc.append(bpc);
    {
        ObjectStore::Transaction t;
        t.write(cid, a, bla.length() + blb.length(), blc.length(), blc, 0);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist final;
    final.append(bla);
    final.append(blb);
    final.append(blc);
    bufferlist actual;
    {
        ASSERT_EQ((int)final.length(),
                  store->read(cid, a, 0, final.length(), actual));
        ASSERT_TRUE(bl_eq(final, actual));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    g_conf->set_val("bluestore_inject_deferred_apply_delay", "0");
    g_ceph_context->_conf->apply_changes(NULL);
}

TEST_P(KvsStoreTest, AppendZeroTrailingSharedBlock) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("fooo", CEPH_NOSNAP)));
    ghobject_t b = a;
    b.hobj.snap = 1;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    unsigned min_alloc = g_conf->bluestore_min_alloc_size;
    unsigned size = min_alloc / 3;
    bufferptr bpa(size);
    memset(bpa.c_str(), 1, bpa.length());
    bufferlist bla;
    bla.append(bpa);
    // make sure there is some trailing gunk in the last block
    {
        bufferlist bt;
        bt.append(bla);
        bt.append("BADBADBADBAD");
        ObjectStore::Transaction t;
        t.write(cid, a, 0, bt.length(), bt, 0);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.truncate(cid, a, size);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    // clone
    {
        ObjectStore::Transaction t;
        t.clone(cid, a, b);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    // append with implicit zeroing
    bufferptr bpb(size);
    memset(bpb.c_str(), 2, bpb.length());
    bufferlist blb;
    blb.append(bpb);
    {
        ObjectStore::Transaction t;
        t.write(cid, a, min_alloc * 3, blb.length(), blb, 0);
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist final;
    final.append(bla);
    bufferlist zeros;
    zeros.append_zero(min_alloc * 3 - size);
    final.append(zeros);
    final.append(blb);
    bufferlist actual;
    {
        ASSERT_EQ((int)final.length(),
                  store->read(cid, a, 0, final.length(), actual));
        final.hexdump(cout);
        actual.hexdump(cout);
        ASSERT_TRUE(bl_eq(final, actual));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove(cid, b);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = store->apply_transaction(&osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SmallSequentialUnaligned) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    int len = 1000;
    bufferptr bp(len);
    bp.zero();
    bl.append(bp);
    for (int i=0; i<1000; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, a, i*len, len, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, ManyBigWrite) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    ghobject_t b(hobject_t(sobject_t("Object 2", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    bufferptr bp(1 * 256*1024);
    bp.zero();
    bl.append(bp);
    for (int i=0; i<8; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, a, i*256*1024, 256*1024, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    // aligned
    for (int i=0; i<8; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, b, (rand() % 8)*1*256*1024, 256*1024, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    // unaligned
    for (int i=0; i<8; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, b, (rand() % (256*8))*1024, 256*1024, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    // do some zeros
    for (int i=0; i<10; ++i) {
        ObjectStore::Transaction t;
        t.zero(cid, b, (rand() % (256*8))*1024, 1048576);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove(cid, b);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, BigWriteBigZero) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("foo", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    bufferptr bp(1048576);
    memset(bp.c_str(), 'b', bp.length());
    bl.append(bp);
    bufferlist s;
    bufferptr sp(4096);
    memset(sp.c_str(), 's', sp.length());
    s.append(sp);
    {
        ObjectStore::Transaction t;
        t.write(cid, a, 0, bl.length(), bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.zero(cid, a, bl.length() / 4, bl.length() / 2);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, a, bl.length() / 2, s.length(), s);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, MiscFragmentTests) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    bufferptr bp(524288);
    bp.zero();
    bl.append(bp);
    {
        ObjectStore::Transaction t;
        t.write(cid, a, 0, 524288, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, a, 1048576, 524288, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bufferlist inbl;
        int r = store->read(cid, a, 524288 + 131072, 1024, inbl);
        ASSERT_EQ(r, 1024);
        ASSERT_EQ(inbl.length(), 1024u);
        ASSERT_TRUE(inbl.is_zero());
    }
    {
        ObjectStore::Transaction t;
        t.write(cid, a, 1048576 - 4096, 524288, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

}


TEST_P(KvsStoreTest, ZeroLengthWrite) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("foo", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, hoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        bufferlist empty;
        t.write(cid, hoid, 1048576, 0, empty);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    struct stat stat;
    r = store->stat(cid, hoid, &stat);
    ASSERT_EQ(0, r);
    ASSERT_EQ(0, stat.st_size);

    bufferlist newdata;
    r = store->read(cid, hoid, 0, 1048576, newdata);
    ASSERT_EQ(0, r);

    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SimpleAttrTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    ghobject_t hoid(hobject_t(sobject_t("attr object 1", CEPH_NOSNAP)));
    bufferlist val, val2;
    val.append("value");
    val.append("value2");
    {
        bufferptr bp;
        map<string,bufferptr> aset;
        r = store->getattr(cid, hoid, "nofoo", bp);
        ASSERT_EQ(-ENOENT, r);
        r = store->getattrs(cid, hoid, aset);
        ASSERT_EQ(-ENOENT, r);
    }
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bool empty;
        int r = store->collection_empty(cid, &empty);
        ASSERT_EQ(0, r);
        ASSERT_TRUE(empty);
    }
    {
        bufferptr bp;
        r = store->getattr(cid, hoid, "nofoo", bp);
        ASSERT_EQ(-ENOENT, r);
    }
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        t.setattr(cid, hoid, "foo", val);
        t.setattr(cid, hoid, "bar", val2);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        bool empty;
        int r = store->collection_empty(cid, &empty);
        ASSERT_EQ(0, r);
        ASSERT_TRUE(!empty);
    }
    {
        bufferptr bp;
        r = store->getattr(cid, hoid, "nofoo", bp);
        ASSERT_EQ(-ENODATA, r);

        r = store->getattr(cid, hoid, "foo", bp);
        ASSERT_EQ(0, r);
        bufferlist bl;
        bl.append(bp);
        ASSERT_TRUE(bl_eq(val, bl));

        map<string,bufferptr> bm;
        r = store->getattrs(cid, hoid, bm);
        ASSERT_EQ(0, r);

    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SimpleListTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid(spg_t(pg_t(0, 1), shard_id_t(1)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    set<ghobject_t> all;
    {
        ObjectStore::Transaction t;
        for (int i=0; i<20; ++i) {
            string name("object_");
            name += stringify(i);
            ghobject_t hoid(hobject_t(sobject_t(name, CEPH_NOSNAP)),
                            ghobject_t::NO_GEN, shard_id_t(1));
            hoid.hobj.pool = 1;
            all.insert(hoid);
            t.touch(cid, hoid);
        }
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        set<ghobject_t> saw;
        vector<ghobject_t> objects;
        ghobject_t next, current;

        while (!next.is_max()) {
            int r = store->collection_list(cid, current, ghobject_t::get_max(),
                                           5,
                                           &objects, &next);
            ASSERT_EQ(r, 0);
            ASSERT_TRUE(sorted(objects));
            for (vector<ghobject_t>::iterator p = objects.begin(); p != objects.end();
                 ++p) {
                if (saw.count(*p)) {
                    //cout << "got DUP " << *p << std::endl;
                } else {
                    //cout << "got new " << *p << std::endl;
                }
                saw.insert(*p);
            }
            objects.clear();
            current = next;
        }
        ASSERT_EQ(saw.size(), all.size());
        ASSERT_EQ(saw, all);
    }
    {
        ObjectStore::Transaction t;
        for (set<ghobject_t>::iterator p = all.begin(); p != all.end(); ++p)
            t.remove(cid, *p);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

}

TEST_P(KvsStoreTest, ListEndTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid(spg_t(pg_t(0, 1), shard_id_t(1)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    set<ghobject_t> all;
    {
        ObjectStore::Transaction t;
        for (int i=0; i<100; ++i) {
            string name("object_");
            name += stringify(i);
            ghobject_t hoid(hobject_t(sobject_t(name, CEPH_NOSNAP)),
                            ghobject_t::NO_GEN, shard_id_t(1));
            hoid.hobj.pool = 1;
            all.insert(hoid);
            t.touch(cid, hoid);
            cerr << "Creating object " << hoid << std::endl;
        }
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ghobject_t end(hobject_t(sobject_t("object_100", CEPH_NOSNAP)),
                       ghobject_t::NO_GEN, shard_id_t(1));
        end.hobj.pool = 1;
        vector<ghobject_t> objects;
        ghobject_t next;
        int r = store->collection_list(cid, ghobject_t(), end, 500,
                                       &objects, &next);
        ASSERT_EQ(r, 0);
        for (auto &p : objects) {
            ASSERT_NE(p, end);
        }
    }
    {
        ObjectStore::Transaction t;
        for (set<ghobject_t>::iterator p = all.begin(); p != all.end(); ++p)
            t.remove(cid, *p);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, Sort) {
    {
        hobject_t a(sobject_t("a", CEPH_NOSNAP));
        hobject_t b = a;
        ASSERT_EQ(a, b);
        b.oid.name = "b";
        ASSERT_NE(a, b);
        ASSERT_TRUE(a < b);
        a.pool = 1;
        b.pool = 2;
        ASSERT_TRUE(a < b);
        a.pool = 3;
        ASSERT_TRUE(a > b);
    }
    {
        ghobject_t a(hobject_t(sobject_t("a", CEPH_NOSNAP)));
        ghobject_t b(hobject_t(sobject_t("b", CEPH_NOSNAP)));
        a.hobj.pool = 1;
        b.hobj.pool = 1;
        ASSERT_TRUE(a < b);
        a.hobj.pool = -3;
        ASSERT_TRUE(a < b);
        a.hobj.pool = 1;
        b.hobj.pool = -3;
        ASSERT_TRUE(a > b);
    }
}

TEST_P(KvsStoreTest, MultipoolListTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    int poolid = 4373;
    coll_t cid = coll_t(spg_t(pg_t(0, poolid), shard_id_t::NO_SHARD));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    set<ghobject_t> all, saw;
    {
        ObjectStore::Transaction t;
        for (int i=0; i<100; ++i) {
            string name("object_");
            name += stringify(i);
            ghobject_t hoid(hobject_t(sobject_t(name, CEPH_NOSNAP)));
            if (rand() & 1)
                hoid.hobj.pool = -2 - poolid;
            else
                hoid.hobj.pool = poolid;
            all.insert(hoid);
            t.touch(cid, hoid);
            cerr << "Creating object " << hoid << std::endl;
        }
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        vector<ghobject_t> objects;
        ghobject_t next, current;
        while (!next.is_max()) {
            int r = store->collection_list(cid, current, ghobject_t::get_max(), 50,
                                           &objects, &next);
            ASSERT_EQ(r, 0);
            cout << " got " << objects.size() << " next " << next << std::endl;
            for (vector<ghobject_t>::iterator p = objects.begin(); p != objects.end();
                 ++p) {
                saw.insert(*p);
            }
            objects.clear();
            current = next;
        }
        ASSERT_EQ(saw, all);
    }
    {
        ObjectStore::Transaction t;
        for (set<ghobject_t>::iterator p = all.begin(); p != all.end(); ++p)
            t.remove(cid, *p);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, SimpleCloneTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP),
                              "key", 123, -1, ""));
    bufferlist small, large, xlarge, newdata, attr;
    small.append("small");
    large.append("large");
    xlarge.append("xlarge");
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        t.setattr(cid, hoid, "attr1", small);
        t.setattr(cid, hoid, "attr2", large);
        t.setattr(cid, hoid, "attr3", xlarge);
        t.write(cid, hoid, 0, small.length(), small);
        t.write(cid, hoid, 10, small.length(), small);
        cerr << "Creating object and set attr " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    ghobject_t hoid2(hobject_t(sobject_t("Object 2", CEPH_NOSNAP),
                               "key", 123, -1, ""));
    ghobject_t hoid3(hobject_t(sobject_t("Object 3", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.clone(cid, hoid, hoid2);
        t.setattr(cid, hoid2, "attr2", small);
        t.rmattr(cid, hoid2, "attr1");
        t.write(cid, hoid, 10, large.length(), large);
        t.setattr(cid, hoid, "attr1", large);
        t.setattr(cid, hoid, "attr2", small);
        cerr << "Clone object and rm attr" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

        r = store->read(cid, hoid, 10, 5, newdata);
        ASSERT_EQ(r, 5);
        ASSERT_TRUE(bl_eq(large, newdata));

        newdata.clear();
        r = store->read(cid, hoid, 0, 5, newdata);
        ASSERT_EQ(r, 5);
        ASSERT_TRUE(bl_eq(small, newdata));

        newdata.clear();
        r = store->read(cid, hoid2, 10, 5, newdata);
        ASSERT_EQ(r, 5);
        ASSERT_TRUE(bl_eq(small, newdata));

        r = store->getattr(cid, hoid2, "attr2", attr);
        ASSERT_EQ(r, 0);
        ASSERT_TRUE(bl_eq(small, attr));

        attr.clear();
        r = store->getattr(cid, hoid2, "attr3", attr);
        ASSERT_EQ(r, 0);
        ASSERT_TRUE(bl_eq(xlarge, attr));

        attr.clear();
        r = store->getattr(cid, hoid, "attr1", attr);
        ASSERT_EQ(r, 0);
        ASSERT_TRUE(bl_eq(large, attr));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
    }
    {
        bufferlist final;
        bufferptr p(16384);
        memset(p.c_str(), 1, p.length());
        bufferlist pl;
        pl.append(p);
        final.append(p);
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, pl.length(), pl);
        t.clone(cid, hoid, hoid2);
        bufferptr a(4096);
        memset(a.c_str(), 2, a.length());
        bufferlist al;
        al.append(a);
        final.append(a);
        t.write(cid, hoid, pl.length(), a.length(), al);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        bufferlist rl;
        ASSERT_EQ((int)final.length(),
                  store->read(cid, hoid, 0, final.length(), rl));
        ASSERT_TRUE(bl_eq(rl, final));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
    }
    {
        bufferlist final;
        bufferptr p(16384);
        memset(p.c_str(), 111, p.length());
        bufferlist pl;
        pl.append(p);
        final.append(p);
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, pl.length(), pl);
        t.clone(cid, hoid, hoid2);
        bufferptr z(4096);
        z.zero();
        final.append(z);
        bufferptr a(4096);
        memset(a.c_str(), 112, a.length());
        bufferlist al;
        al.append(a);
        final.append(a);
        t.write(cid, hoid, pl.length() + z.length(), a.length(), al);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
        bufferlist rl;
        ASSERT_EQ((int)final.length(),
                  store->read(cid, hoid, 0, final.length(), rl));
        ASSERT_TRUE(bl_eq(rl, final));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
    }
    {
        bufferlist final;
        bufferptr p(16000);
        memset(p.c_str(), 5, p.length());
        bufferlist pl;
        pl.append(p);
        final.append(p);
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, pl.length(), pl);
        t.clone(cid, hoid, hoid2);
        bufferptr z(1000);
        z.zero();
        final.append(z);
        bufferptr a(8000);
        memset(a.c_str(), 6, a.length());
        bufferlist al;
        al.append(a);
        final.append(a);
        t.write(cid, hoid, 17000, a.length(), al);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
        bufferlist rl;
        ASSERT_EQ((int)final.length(),
                  store->read(cid, hoid, 0, final.length(), rl));
        ASSERT_TRUE(bl_eq(rl, final));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
    }
    {
        bufferptr p(1048576);
        memset(p.c_str(), 3, p.length());
        bufferlist pl;
        pl.append(p);
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, pl.length(), pl);
        t.clone(cid, hoid, hoid2);
        bufferptr a(65536);
        memset(a.c_str(), 4, a.length());
        bufferlist al;
        al.append(a);
        t.write(cid, hoid, a.length(), a.length(), al);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
        bufferlist rl;
        bufferlist final;
        final.substr_of(pl, 0, al.length());
        final.append(al);
        bufferlist end;
        end.substr_of(pl, al.length()*2, pl.length() - al.length()*2);
        final.append(end);
        ASSERT_EQ((int)final.length(),
                  store->read(cid, hoid, 0, final.length(), rl));
        ASSERT_TRUE(bl_eq(rl, final));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
    }
    {
        bufferptr p(65536);
        memset(p.c_str(), 7, p.length());
        bufferlist pl;
        pl.append(p);
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, pl.length(), pl);
        t.clone(cid, hoid, hoid2);
        bufferptr a(4096);
        memset(a.c_str(), 8, a.length());
        bufferlist al;
        al.append(a);
        t.write(cid, hoid, 32768, a.length(), al);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
        bufferlist rl;
        bufferlist final;
        final.substr_of(pl, 0, 32768);
        final.append(al);
        bufferlist end;
        end.substr_of(pl, final.length(), pl.length() - final.length());
        final.append(end);
        ASSERT_EQ((int)final.length(),
                    store->read(cid, hoid, 0, final.length(), rl));
        ASSERT_TRUE(bl_eq(rl, final));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
    }
    {
        bufferptr p(65536);
        memset(p.c_str(), 9, p.length());
        bufferlist pl;
        pl.append(p);
        ObjectStore::Transaction t;
        t.write(cid, hoid, 0, pl.length(), pl);
        t.clone(cid, hoid, hoid2);
        bufferptr a(4096);
        memset(a.c_str(), 10, a.length());
        bufferlist al;
        al.append(a);
        t.write(cid, hoid, 33768, a.length(), al);
        ASSERT_EQ(0, apply_transaction(store, &osr, std::move(t)));
        bufferlist rl;
        bufferlist final;
        final.substr_of(pl, 0, 33768);
        final.append(al);
        bufferlist end;
        end.substr_of(pl, final.length(), pl.length() - final.length());
        final.append(end);
        ASSERT_EQ((int)final.length(),
                  store->read(cid, hoid, 0, final.length(), rl));
        /*cout << "expected:\n";
    final.hexdump(cout);
    cout << "got:\n";
    rl.hexdump(cout);*/
        ASSERT_TRUE(bl_eq(rl, final));
    }

    //Unfortunately we need a workaround for filestore since EXPECT_DEATH
    // macro has potential issues when using /in multithread environments. 
    //It works well for all stores but filestore for now. 
    //A fix setting gtest_death_test_style = "threadsafe" doesn't help as well - 
    //  test app clone asserts on store folder presence.
    //
    if (string(GetParam()) != "kvsstore") {
        //verify if non-empty collection is properly handled after store reload
        r = store->umount();
        ASSERT_EQ(r, 0);
        r = store->mount();
        ASSERT_EQ(r, 0);

        ObjectStore::Transaction t;
        t.remove_collection(cid);
        cerr << "Invalid rm coll" << std::endl;
        PrCtl unset_dumpable;
        EXPECT_DEATH(apply_transaction(store, &osr, std::move(t)), "");
    }
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid3); //new record in db
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    //See comment above for "filestore" check explanation.
    if (string(GetParam()) != "kvsstore") {
        ObjectStore::Transaction t;
        //verify if non-empty collection is properly handled when there are some pending removes and live records in db
        cerr << "Invalid rm coll again" << std::endl;
        r = store->umount();
        ASSERT_EQ(r, 0);
        r = store->mount();
        ASSERT_EQ(r, 0);

        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        t.remove_collection(cid);
        PrCtl unset_dumpable;
        EXPECT_DEATH(apply_transaction(store, &osr, std::move(t)), "");
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove(cid, hoid2);
        t.remove(cid, hoid3);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}



TEST_P(KvsStoreTest, SimpleObjectLongnameTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ghobject_t hoid(hobject_t(sobject_t("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaObjectaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        cerr << "Creating object " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

ghobject_t generate_long_name(unsigned i)
{
    stringstream name;
    name << "object id " << i << " ";
    for (unsigned j = 0; j < 180; ++j) name << 'a';
    ghobject_t hoid(hobject_t(sobject_t(name.str(), CEPH_NOSNAP)));
    hoid.hobj.set_hash(i % 2);
    return hoid;
}

TEST_P(KvsStoreTest, LongnameSplitTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(0, r);
    }
    for (unsigned i = 0; i < 320; ++i) {
        ObjectStore::Transaction t;
        ghobject_t hoid = generate_long_name(i);
        t.touch(cid, hoid);
        cerr << "Creating object " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(0, r);
    }

    ghobject_t test_obj = generate_long_name(319);
    ghobject_t test_obj_2 = test_obj;
    test_obj_2.generation = 0;
    {
        ObjectStore::Transaction t;
        // should cause a split
        t.collection_move_rename(
                cid, test_obj,
                cid, test_obj_2);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(0, r);
    }

    for (unsigned i = 0; i < 319; ++i) {
        ObjectStore::Transaction t;
        ghobject_t hoid = generate_long_name(i);
        t.remove(cid, hoid);
        cerr << "Removing object " << hoid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(0, r);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, test_obj_2);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(0, r);
    }

}



TEST_P(KvsStoreTestSpecificAUSize, ZipperPatternSharded) {
    if(string(GetParam()) != "bluestore")
        return;
    StartDeferred(4096);

    int r;
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t a(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    int len = 4096;
    bufferptr bp(len);
    bp.zero();
    bl.append(bp);
    for (int i=0; i<1000; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, a, i*2*len, len, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    for (int i=0; i<1000; ++i) {
        ObjectStore::Transaction t;
        t.write(cid, a, i*2*len + 1, len, bl, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, a);
        t.remove_collection(cid);
        cerr << "Cleaning" << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, AttrSynthetic) {
    ObjectStore::Sequencer osr("test");
    MixedGenerator gen(447);
    gen_type rng(time(NULL));
    coll_t cid(spg_t(pg_t(0,447),shard_id_t::NO_SHARD));

    SyntheticWorkloadState test_obj(store.get(), &gen, &rng, &osr, cid, 40*1024, 4*1024, 0);
    test_obj.init();
    for (int i = 0; i < 500; ++i) {
        if (!(i % 10)) cerr << "seeding object " << i << std::endl;
        test_obj.touch();
    }
    for (int i = 0; i < 1000; ++i) {
        if (!(i % 100)) {
            cerr << "Op " << i << std::endl;
            test_obj.print_internal_state();
        }
        boost::uniform_int<> true_false(0, 99);
        int val = true_false(rng);
        if (val > 97) {
            test_obj.scan();
        } else if (val > 93) {
            test_obj.stat();
        } else if (val > 75) {
            test_obj.rmattr();
        } else if (val > 47) {
            test_obj.setattrs();
        } else if (val > 45) {
            test_obj.clone();
        } else if (val > 37) {
            //test_obj.stash();
        } else if (val > 30) {
            test_obj.getattrs();
        } else {
            test_obj.getattr();
        }
    }
    test_obj.wait_for_done();
    test_obj.shutdown();
}

TEST_P(KvsStoreTest, HashCollisionTest) {
    ObjectStore::Sequencer osr("test");
    int64_t poolid = 11;
    coll_t cid(spg_t(pg_t(0,poolid),shard_id_t::NO_SHARD));
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    string base = "";
    for (int i = 0; i < 15; ++i) base.append("aaaaa");
    set<ghobject_t> created;
    for (int n = 0; n < 10; ++n) {
        char nbuf[100];
        sprintf(nbuf, "n%d", n);
        for (int i = 0; i < 1000; ++i) {
            char buf[100];
            sprintf(buf, "%d", i);
            if (!(i % 100)) {
                cerr << "Object n" << n << " "<< i << std::endl;
            }
            ghobject_t hoid(hobject_t(string(buf) + base, string(), CEPH_NOSNAP, 0, poolid, string(nbuf)));
            {
                ObjectStore::Transaction t;
                t.touch(cid, hoid);
                r = apply_transaction(store, &osr, std::move(t));
                ASSERT_EQ(r, 0);
            }
            created.insert(hoid);
        }
    }
    vector<ghobject_t> objects;
    r = store->collection_list(cid, ghobject_t(), ghobject_t::get_max(), INT_MAX, &objects, 0);
    ASSERT_EQ(r, 0);
    set<ghobject_t> listed(objects.begin(), objects.end());
    cerr << "listed.size() is " << listed.size() << " and created.size() is " << created.size() << std::endl;
    ASSERT_TRUE(listed.size() == created.size());
    objects.clear();
    listed.clear();
    ghobject_t current, next;
    while (1) {
        r = store->collection_list(cid, current, ghobject_t::get_max(), 60,
                                   &objects, &next);
        ASSERT_EQ(r, 0);
        ASSERT_TRUE(sorted(objects));
        for (vector<ghobject_t>::iterator i = objects.begin();
             i != objects.end();
             ++i) {
            if (listed.count(*i))
                cerr << *i << " repeated" << std::endl;
            listed.insert(*i);
        }
        if (objects.size() < 50) {
            ASSERT_TRUE(next.is_max());
            break;
        }
        objects.clear();
        current = next;
    }
    cerr << "listed.size() is " << listed.size() << std::endl;
    ASSERT_TRUE(listed.size() == created.size());
    for (set<ghobject_t>::iterator i = listed.begin();
         i != listed.end();
         ++i) {
        ASSERT_TRUE(created.count(*i));
    }

    for (set<ghobject_t>::iterator i = created.begin();
         i != created.end();
         ++i) {
        ObjectStore::Transaction t;
        t.remove(cid, *i);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ObjectStore::Transaction t;
    t.remove_collection(cid);
    r = apply_transaction(store, &osr, std::move(t));
    ASSERT_EQ(r, 0);
}

TEST_P(KvsStoreTest, ScrubTest) {
    ObjectStore::Sequencer osr("test");
    int64_t poolid = 111;
    coll_t cid(spg_t(pg_t(0, poolid),shard_id_t(1)));
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    string base = "aaaaa";
    set<ghobject_t> created;
    for (int i = 0; i < 1000; ++i) {
        char buf[100];
        sprintf(buf, "%d", i);
        if (!(i % 5)) {
            cerr << "Object " << i << std::endl;
        }
        ghobject_t hoid(hobject_t(string(buf) + base, string(), CEPH_NOSNAP, i,
                poolid, ""),
        ghobject_t::NO_GEN, shard_id_t(1));
        {
            ObjectStore::Transaction t;
            t.touch(cid, hoid);
            r = apply_transaction(store, &osr, std::move(t));
            ASSERT_EQ(r, 0);
        }
        created.insert(hoid);
    }

    // Add same hobject_t but different generation
    {
        ghobject_t hoid1(hobject_t("same-object", string(), CEPH_NOSNAP, 0, poolid, ""),
                         ghobject_t::NO_GEN, shard_id_t(1));
        ghobject_t hoid2(hobject_t("same-object", string(), CEPH_NOSNAP, 0, poolid, ""), (gen_t)1, shard_id_t(1));
        ghobject_t hoid3(hobject_t("same-object", string(), CEPH_NOSNAP, 0, poolid, ""), (gen_t)2, shard_id_t(1));
        ObjectStore::Transaction t;
        t.touch(cid, hoid1);
        t.touch(cid, hoid2);
        t.touch(cid, hoid3);
        r = apply_transaction(store, &osr, std::move(t));
        created.insert(hoid1);
        created.insert(hoid2);
        created.insert(hoid3);
        ASSERT_EQ(r, 0);
    }

    vector<ghobject_t> objects;
    r = store->collection_list(cid, ghobject_t(), ghobject_t::get_max(),
                               INT_MAX, &objects, 0);
    ASSERT_EQ(r, 0);
    set<ghobject_t> listed(objects.begin(), objects.end());
    cerr << "listed.size() is " << listed.size() << " and created.size() is " << created.size() << std::endl;
    ASSERT_TRUE(listed.size() == created.size());
    objects.clear();
    listed.clear();
    ghobject_t current, next;
    while (1) {
        r = store->collection_list(cid, current, ghobject_t::get_max(), 60,
                                   &objects, &next);
        ASSERT_EQ(r, 0);
        ASSERT_TRUE(sorted(objects));
        for (vector<ghobject_t>::iterator i = objects.begin();
             i != objects.end(); ++i) {
            if (listed.count(*i))
                cerr << *i << " repeated" << std::endl;
            listed.insert(*i);
        }
        if (objects.size() < 50) {
            ASSERT_TRUE(next.is_max());
            break;
        }
        objects.clear();
        current = next.get_boundary();
    }
    cerr << "listed.size() is " << listed.size() << std::endl;
    ASSERT_TRUE(listed.size() == created.size());
    for (set<ghobject_t>::iterator i = listed.begin();
         i != listed.end();
         ++i) {
        ASSERT_TRUE(created.count(*i));
    }

    for (set<ghobject_t>::iterator i = created.begin();
         i != created.end();
         ++i) {
        ObjectStore::Transaction t;
        t.remove(cid, *i);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ObjectStore::Transaction t;
    t.remove_collection(cid);
    r = apply_transaction(store, &osr, std::move(t));
    ASSERT_EQ(r, 0);
}






TEST_P(KvsStoreTest, OMapIterator) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t("tesomap", "", CEPH_NOSNAP, 0, 0, ""));
    int count = 0;
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    map<string, bufferlist> attrs;
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        t.omap_clear(cid, hoid);
        map<string, bufferlist> start_set;
        t.omap_setkeys(cid, hoid, start_set);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ObjectMap::ObjectMapIterator iter;
    bool correct;
    //basic iteration
    for (int i = 0; i < 100; i++) {
        if (!(i%5)) {
            std::cout << "On iteration " << i << std::endl;
        }
        bufferlist bl;

        // FileStore may deadlock two active iterators over the same data
        iter = ObjectMap::ObjectMapIterator();

        iter = store->get_omap_iterator(cid, hoid);
        for (iter->seek_to_first(), count=0; iter->valid(); iter->next(), count++) {
            string key = iter->key();
            bufferlist value = iter->value();
            correct = attrs.count(key) && (string(value.c_str()) == string(attrs[key].c_str()));
            if (!correct) {
                if (attrs.count(key) > 0) {
                    std::cout << "key " << key << "in omap , " << value.c_str() << " : " << attrs[key].c_str() << std::endl;
                }
                else
                    std::cout << "key " << key << "should not exists in omap" << std::endl;
            }
            ASSERT_EQ(correct, true);
        }
        ASSERT_EQ((int)attrs.size(), count);

        // FileStore may deadlock an active iterator vs apply_transaction
        iter = ObjectMap::ObjectMapIterator();

        char buf[100];
        snprintf(buf, sizeof(buf), "%d", i);
        bl.clear();
        bufferptr bp(buf, strlen(buf) + 1);
        bl.append(bp);
        map<string, bufferlist> to_add;
        to_add.insert(pair<string, bufferlist>("key-" + string(buf), bl));
        attrs.insert(pair<string, bufferlist>("key-" + string(buf), bl));
        ObjectStore::Transaction t;
        t.omap_setkeys(cid, hoid, to_add);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    iter = store->get_omap_iterator(cid, hoid);
    //lower bound
    string bound_key = "key-5";
    iter->lower_bound(bound_key);
    correct = bound_key <= iter->key();
    if (!correct) {
        std::cout << "lower bound, bound key is " << bound_key << " < iter key is " << iter->key() << std::endl;
    }
    ASSERT_EQ(correct, true);
    //upper bound
    iter->upper_bound(bound_key);
    correct = iter->key() > bound_key;
    if (!correct) {
        std::cout << "upper bound, bound key is " << bound_key << " >= iter key is " << iter->key() << std::endl;
    }
    ASSERT_EQ(correct, true);

    // FileStore may deadlock an active iterator vs apply_transaction
    iter = ObjectMap::ObjectMapIterator();
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, XattrTest) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t("tesomap", "", CEPH_NOSNAP, 0, 0, ""));
    bufferlist big;
    for (unsigned i = 0; i < 10000; ++i) {
        big.append('\0');
    }
    bufferlist small;
    for (unsigned i = 0; i < 10; ++i) {
        small.append('\0');
    }
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, hoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    map<string, bufferlist> attrs;
    {
        ObjectStore::Transaction t;
        t.setattr(cid, hoid, "attr1", small);
        attrs["attr1"] = small;
        t.setattr(cid, hoid, "attr2", big);
        attrs["attr2"] = big;
        t.setattr(cid, hoid, "attr3", small);
        attrs["attr3"] = small;
        t.setattr(cid, hoid, "attr1", small);
        attrs["attr1"] = small;
        t.setattr(cid, hoid, "attr4", big);
        attrs["attr4"] = big;
        t.setattr(cid, hoid, "attr3", big);
        attrs["attr3"] = big;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    map<string, bufferptr> aset;
    store->getattrs(cid, hoid, aset);
    ASSERT_EQ(aset.size(), attrs.size());
    for (map<string, bufferptr>::iterator i = aset.begin();
         i != aset.end();
         ++i) {
        bufferlist bl;
        bl.push_back(i->second);
        ASSERT_TRUE(attrs[i->first] == bl);
    }

    {
        ObjectStore::Transaction t;
        t.rmattr(cid, hoid, "attr2");
        attrs.erase("attr2");
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    aset.clear();
    store->getattrs(cid, hoid, aset);
    ASSERT_EQ(aset.size(), attrs.size());
    for (map<string, bufferptr>::iterator i = aset.begin();
         i != aset.end();
         ++i) {
        bufferlist bl;
        bl.push_back(i->second);
        ASSERT_TRUE(attrs[i->first] == bl);
    }

    bufferptr bp;
    r = store->getattr(cid, hoid, "attr2", bp);
    ASSERT_EQ(r, -ENODATA);

    r = store->getattr(cid, hoid, "attr3", bp);
    ASSERT_EQ(r, 0);
    bufferlist bl2;
    bl2.push_back(bp);
    ASSERT_TRUE(bl2 == attrs["attr3"]);

    ObjectStore::Transaction t;
    t.remove(cid, hoid);
    t.remove_collection(cid);
    r = apply_transaction(store, &osr, std::move(t));
    ASSERT_EQ(r, 0);
}



/**
 * This test tests adding two different groups
 * of objects, each with 1 common prefix and 1
 * different prefix.  We then remove half
 * in order to verify that the merging correctly
 * stops at the common prefix subdir.  See bug
 * #5273 */
TEST_P(KvsStoreTest, TwoHash) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    std::cout << "Making objects" << std::endl;
    for (int i = 0; i < 360; ++i) {
        ObjectStore::Transaction t;
        ghobject_t o;
        o.hobj.pool = -1;
        if (i < 8) {
            o.hobj.set_hash((i << 16) | 0xA1);
            t.touch(cid, o);
        }
        o.hobj.set_hash((i << 16) | 0xB1);
        t.touch(cid, o);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    std::cout << "Removing half" << std::endl;
    for (int i = 1; i < 8; ++i) {
        ObjectStore::Transaction t;
        ghobject_t o;
        o.hobj.pool = -1;
        o.hobj.set_hash((i << 16) | 0xA1);
        t.remove(cid, o);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    std::cout << "Checking" << std::endl;
    for (int i = 1; i < 8; ++i) {
        ObjectStore::Transaction t;
        ghobject_t o;
        o.hobj.set_hash((i << 16) | 0xA1);
        o.hobj.pool = -1;
        bool exists = store->exists(cid, o);
        ASSERT_EQ(exists, false);
    }
    {
        ghobject_t o;
        o.hobj.set_hash(0xA1);
        o.hobj.pool = -1;
        bool exists = store->exists(cid, o);
        ASSERT_EQ(exists, true);
    }
    std::cout << "Cleanup" << std::endl;
    for (int i = 0; i < 360; ++i) {
        ObjectStore::Transaction t;
        ghobject_t o;
        o.hobj.set_hash((i << 16) | 0xA1);
        o.hobj.pool = -1;
        t.remove(cid, o);
        o.hobj.set_hash((i << 16) | 0xB1);
        t.remove(cid, o);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ObjectStore::Transaction t;
    t.remove_collection(cid);
    r = apply_transaction(store, &osr, std::move(t));
    ASSERT_EQ(r, 0);
}

TEST_P(KvsStoreTest, Rename) {
    ObjectStore::Sequencer osr("test");
    coll_t cid(spg_t(pg_t(0, 2122),shard_id_t::NO_SHARD));
    ghobject_t srcoid(hobject_t("src_oid", "", CEPH_NOSNAP, 0, 0, ""));
    ghobject_t dstoid(hobject_t("dest_oid", "", CEPH_NOSNAP, 0, 0, ""));
    bufferlist a, b;
    a.append("foo");
    b.append("bar");
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.write(cid, srcoid, 0, a.length(), a);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ASSERT_TRUE(store->exists(cid, srcoid));
    {
        ObjectStore::Transaction t;
        t.collection_move_rename(cid, srcoid, cid, dstoid);
        t.write(cid, srcoid, 0, b.length(), b);
        t.setattr(cid, srcoid, "attr", b);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ASSERT_TRUE(store->exists(cid, srcoid));
    ASSERT_TRUE(store->exists(cid, dstoid));
    {
        bufferlist bl;
        store->read(cid, srcoid, 0, 3, bl);
        ASSERT_TRUE(bl_eq(b, bl));
        store->read(cid, dstoid, 0, 3, bl);
        ASSERT_TRUE(bl_eq(a, bl));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, dstoid);
        t.collection_move_rename(cid, srcoid, cid, dstoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ASSERT_TRUE(store->exists(cid, dstoid));
    ASSERT_FALSE(store->exists(cid, srcoid));
    {
        bufferlist bl;
        store->read(cid, dstoid, 0, 3, bl);
        ASSERT_TRUE(bl_eq(b, bl));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, dstoid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, MoveRename) {
    ObjectStore::Sequencer osr("test");
    coll_t cid(spg_t(pg_t(0, 212),shard_id_t::NO_SHARD));
    ghobject_t temp_oid(hobject_t("tmp_oid", "", CEPH_NOSNAP, 0, 0, ""));
    ghobject_t oid(hobject_t("dest_oid", "", CEPH_NOSNAP, 0, 0, ""));
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, oid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ASSERT_TRUE(store->exists(cid, oid));
    bufferlist data, attr;
    map<string, bufferlist> omap;
    data.append("data payload");
    attr.append("attr value");
    omap["omap_key"].append("omap value");
    {
        ObjectStore::Transaction t;
        t.touch(cid, temp_oid);
        t.write(cid, temp_oid, 0, data.length(), data);
        t.setattr(cid, temp_oid, "attr", attr);
        t.omap_setkeys(cid, temp_oid, omap);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ASSERT_TRUE(store->exists(cid, temp_oid));
    {
        ObjectStore::Transaction t;
        t.remove(cid, oid);
        t.collection_move_rename(cid, temp_oid, cid, oid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    ASSERT_TRUE(store->exists(cid, oid));
    ASSERT_FALSE(store->exists(cid, temp_oid));
    {
        bufferlist newdata;
        r = store->read(cid, oid, 0, 1000, newdata);
        ASSERT_GE(r, 0);
        ASSERT_TRUE(bl_eq(data, newdata));
        bufferlist newattr;
        r = store->getattr(cid, oid, "attr", newattr);
        ASSERT_EQ(r, 0);
        ASSERT_TRUE(bl_eq(attr, newattr));
        set<string> keys;
        keys.insert("omap_key");
        map<string, bufferlist> newomap;
        r = store->omap_get_values(cid, oid, keys, &newomap);
        ASSERT_GE(r, 0);
        ASSERT_EQ(1u, newomap.size());
        ASSERT_TRUE(newomap.count("omap_key"));
        ASSERT_TRUE(bl_eq(omap["omap_key"], newomap["omap_key"]));
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, oid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, BigRGWObjectName) {
    ObjectStore::Sequencer osr("test");
    coll_t cid(spg_t(pg_t(0,12),shard_id_t::NO_SHARD));
    ghobject_t oid(
            hobject_t(
                    "default.4106.50_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    "",
                    CEPH_NOSNAP,
                    0x81920472,
                    12,
                    ""),
            15,
            shard_id_t::NO_SHARD);
    ghobject_t oid2(oid);
    oid2.generation = 17;
    ghobject_t oidhead(oid);
    oidhead.generation = ghobject_t::NO_GEN;

    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, oidhead);
        t.collection_move_rename(cid, oidhead, cid, oid);
        t.touch(cid, oidhead);
        t.collection_move_rename(cid, oidhead, cid, oid2);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    {
        ObjectStore::Transaction t;
        t.remove(cid, oid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    {
        vector<ghobject_t> objects;
        r = store->collection_list(cid, ghobject_t(), ghobject_t::get_max(),
                                   INT_MAX, &objects, 0);
        ASSERT_EQ(r, 0);
        ASSERT_EQ(objects.size(), 1u);
        ASSERT_EQ(objects[0], oid2);
    }

    ASSERT_FALSE(store->exists(cid, oid));

    {
        ObjectStore::Transaction t;
        t.remove(cid, oid2);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);

    }
}

TEST_P(KvsStoreTest, SetAllocHint) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t("test_hint", "", CEPH_NOSNAP, 0, 0, ""));
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        t.touch(cid, hoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.set_alloc_hint(cid, hoid, 4*1024*1024, 1024*4, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.set_alloc_hint(cid, hoid, 4*1024*1024, 1024*4, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}

TEST_P(KvsStoreTest, TryMoveRename) {
    ObjectStore::Sequencer osr("test");
    coll_t cid;
    ghobject_t hoid(hobject_t("test_hint", "", CEPH_NOSNAP, 0, -1, ""));
    ghobject_t hoid2(hobject_t("test_hint2", "", CEPH_NOSNAP, 0, -1, ""));
    int r;
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.try_rename(cid, hoid, hoid2);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.touch(cid, hoid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        t.try_rename(cid, hoid, hoid2);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    struct stat st;
    ASSERT_EQ(store->stat(cid, hoid, &st), -ENOENT);
    ASSERT_EQ(store->stat(cid, hoid2, &st), 0);
}

void doReadAfterIterateTest(boost::scoped_ptr<ObjectStore>& store,
                     int num_ops,
                     uint64_t max_obj, uint64_t max_wr, uint64_t align) {
    ObjectStore::Sequencer osr("test");
    MixedGenerator gen(555);
    gen_type rng(time(NULL));
    coll_t cid(spg_t(pg_t(0, 555), shard_id_t::NO_SHARD));

    g_ceph_context->_conf->set_val("bluestore_fsck_on_mount", "false");
    g_ceph_context->_conf->set_val("bluestore_fsck_on_umount", "false");
    g_ceph_context->_conf->apply_changes(NULL);

    SyntheticWorkloadState test_obj(store.get(), &gen, &rng, &osr, cid,
                                    max_obj, max_wr, align);
    test_obj.init();
    for (int i = 0; i < num_ops / 10; ++i) {
        if (!(i % 500)) cerr << "seeding object " << i << std::endl;
        test_obj.touch();

    }

    test_obj.wait_for_done();

    derr << test_obj.available_objects.size() << " keys are touched - written" << dendl;

    // check if it happens with non zero keys
     
    test_obj.wait_for_done();
    
    bufferlist value;
    value.append_zero(4096);
    for (const auto obj : test_obj.available_objects) {
        ObjectStore::Transaction t;
        t.write(cid, obj, 0, 4096, value);
        store->apply_transaction(&osr, std::move(t));
    }
   
    derr << test_obj.available_objects.size() << " keys are written" << dendl;
  
    
    for (int i = 0; i < 1; i++) {
        derr << "SCAN #" << i << dendl;
        test_obj.scan2();
    }

    test_obj.wait_for_done();
    derr << "scan done" << dendl;
    
    // check if all keys are available.
    for (int i =0; i< 1; i++) {
        bufferlist result;
        for (const auto obj : test_obj.available_objects) {
            bufferlist bl, result;
            int r = 0;
            r = store->read(test_obj.cid, obj, 0, 4096, result);

            if (r < 0) {
                derr << "error = " << r << ", oid = " << obj << dendl;
            }
        }
    }
    
    test_obj.wait_for_done();
    derr << " ALL done "<< dendl;
    
    test_obj.shutdown();

}



void zerosizedObjects(boost::scoped_ptr<ObjectStore>& store,
                     int num_ops,
                     uint64_t max_obj, uint64_t max_wr, uint64_t align){
    ObjectStore::Sequencer osr("test");
    MixedGenerator gen(555);
    gen_type rng(time(NULL));
    coll_t cid(spg_t(pg_t(0, 555), shard_id_t::NO_SHARD));

    g_ceph_context->_conf->set_val("bluestore_fsck_on_mount", "false");
    g_ceph_context->_conf->set_val("bluestore_fsck_on_umount", "false");
    g_ceph_context->_conf->apply_changes(NULL);

    SyntheticWorkloadState test_obj(store.get(), &gen, &rng, &osr, cid,
                                    max_obj, max_wr, align);
    test_obj.init();

    derr << " EMPTY collection_list test " << dendl;

    test_obj.scan2();
   
    derr << "[DONE] EMPTY collection_list test " << dendl;

    for (int i = 0; i < num_ops / 10; ++i) {
        if (!(i % 500)) cerr << "seeding object " << i << std::endl;
        test_obj.touch();

    }


    test_obj.wait_for_done();

    derr << test_obj.available_objects.size() << " keys are touched - written" << dendl;

    // check if it happens with non zero keys
     
    test_obj.wait_for_done();

    for (int i = 0; i < 1; i++) {
        derr << "SCAN #" << i << dendl;
        test_obj.scan2();
    }

    test_obj.wait_for_done();
    derr << "scan done" << dendl;
    
    // check if all keys are available.
    for (int i =0; i< 1; i++) {
        bufferlist result;
        for (const auto obj : test_obj.available_objects) {
            bufferlist bl, result;
            int r = 0;

            r = store->read(test_obj.cid, obj, 0, 1, result);

            if (r < 0) {
                derr << "error = " << r << ", oid = " << obj << dendl;
            }
        }
    }
    
    test_obj.wait_for_done();
    derr << " ALL done "<< dendl;
    
    test_obj.shutdown();
}


void doMany4KWritesTest(boost::scoped_ptr<ObjectStore>& store,
                        unsigned max_objects,
                        unsigned max_ops,
                        unsigned max_object_size,
                        unsigned max_write_size,
                        unsigned write_alignment,
                        store_statfs_t* res_stat)
{
    ObjectStore::Sequencer osr("test");
    MixedGenerator gen(555);
    gen_type rng(time(NULL));
    coll_t cid(spg_t(pg_t(0,555), shard_id_t::NO_SHARD));

    SyntheticWorkloadState test_obj(store.get(),
                                    &gen,
                                    &rng,
                                    &osr,
                                    cid,
                                    max_object_size,
                                    max_write_size,
                                    write_alignment);
    test_obj.init();
    for (unsigned i = 0; i < max_objects; ++i) {
        if (!(i % 500)) cerr << "seeding object " << i << std::endl;
        test_obj.touch();
    }
    for (unsigned i = 0; i < max_ops; ++i) {
        if (!(i % 200)) {
            cerr << "Op " << i << std::endl;
            test_obj.print_internal_state();
        }
        test_obj.write();
    }
    test_obj.wait_for_done();
    if (res_stat) {
        test_obj.statfs(*res_stat);
    }
    test_obj.shutdown();
}

TEST_P(KvsStoreTest, CollectionListTest1){
    derr << "zero sized objects: "<< dendl;
    zerosizedObjects(store, 10000, 400*1024, 40*1024, 0);
    derr << "[DONE] zero sized objects"<< dendl;
    derr << "Rigour Test collection_list "<< dendl;
    doReadAfterIterateTest(store, 10000, 400*1024, 40*1024, 0);
    derr << " [DONE] Rigour test collection_list "<< dendl;
}


TEST_P(KvsStoreTest, ColSplitTest1) {
    colsplittest(store.get(), 100, 7, false);
    //colsplittest(store.get(), 10000, 11, false);
}
TEST_P(KvsStoreTest, ColSplitTest1Clones) {
    colsplittest(store.get(), 100, 11, true);
}
TEST_P(KvsStoreTest, ColSplitTest2) {
    colsplittest(store.get(), 100, 7, false);
}
TEST_P(KvsStoreTest, ColSplitTest2Clones) {
    colsplittest(store.get(), 100, 7, true);
}
TEST_P(KvsStoreTest, ColSplitTest3) {
    colsplittest(store.get(), 100, 25, false);
}

TEST_P(KvsStoreTest, DataConsistencyTest) {
    ObjectStore::Sequencer osr("test");
    int r;
    coll_t cid(spg_t(pg_t(0, 55), shard_id_t::NO_SHARD));
    ghobject_t hoid(hobject_t(sobject_t("Object 1", CEPH_NOSNAP)));
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        cerr << "Creating collection " << cid << std::endl;
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    bufferlist bl;
    {
        ObjectStore::Transaction t;

        bl.append("abcdeabcdeabcdeabcdeabcdeabcdea");
	t.touch(cid, hoid);
        t.write(cid, hoid, 0, 31, bl);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    {
        ObjectStore::Transaction t;
        bufferlist in;
        r = store->read(cid, hoid, 0, 100, in);
        ASSERT_EQ(31, r);
        ASSERT_TRUE(bl_eq(bl, in));
    }
    derr << "cleaning..." << dendl;
    {
        ObjectStore::Transaction t;
        t.remove(cid, hoid);
        t.remove_collection(cid);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
}


TEST_P(KvsStoreTest, Synthetic) {
    doSyntheticTest(store, 10000, 400*1024, 40*1024, 0);
}


#if BLUESTORE_TESTS
/// ----------------------------------------------------------------------
/// Bluestore-only tests
/// ----------------------------------------------------------------------

TEST_P(KvsStoreTestSpecificAUSize, Many4KWritesTest) {
    if (string(GetParam()) != "bluestore")
        return;

    StartDeferred(0x10000);

    store_statfs_t res_stat;
    unsigned max_object = 4*1024*1024;

    doMany4KWritesTest(store, 1, 1000, 4*1024*1024, 4*1024, 0, &res_stat);

    ASSERT_LE(res_stat.stored, max_object);
    ASSERT_EQ(res_stat.allocated, max_object);
}

TEST_P(KvsStoreTestSpecificAUSize, Many4KWritesNoCSumTest) {
    if (string(GetParam()) != "bluestore")
        return;
    StartDeferred(0x10000);
    g_conf->set_val("bluestore_csum_type", "none");
    g_ceph_context->_conf->apply_changes(NULL);
    store_statfs_t res_stat;
    unsigned max_object = 4*1024*1024;

    doMany4KWritesTest(store, 1, 1000, max_object, 4*1024, 0, &res_stat );

    ASSERT_LE(res_stat.stored, max_object);
    ASSERT_EQ(res_stat.allocated, max_object);
    g_conf->set_val("bluestore_csum_type", "crc32c");
}

TEST_P(KvsStoreTestSpecificAUSize, TooManyBlobsTest) {
    if (string(GetParam()) != "bluestore")
        return;
    StartDeferred(0x10000);
    store_statfs_t res_stat;
    unsigned max_object = 4*1024*1024;
    doMany4KWritesTest(store, 1, 1000, max_object, 4*1024, 0, &res_stat);
    ASSERT_LE(res_stat.stored, max_object);
    ASSERT_EQ(res_stat.allocated, max_object);
}


TEST_P(KvsStoreTest, KVDBHistogramTest) {
    if (string(GetParam()) != "bluestore")
        return;

    ObjectStore::Sequencer osr("test");
    int NUM_OBJS = 200;
    int r = 0;
    coll_t cid;
    string base("testobj.");
    bufferlist a;
    bufferptr ap(0x1000);
    memset(ap.c_str(), 'a', 0x1000);
    a.append(ap);
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    for (int i = 0; i < NUM_OBJS; ++i) {
        ObjectStore::Transaction t;
        char buf[100];
        snprintf(buf, sizeof(buf), "%d", i);
        ghobject_t hoid(hobject_t(sobject_t(base + string(buf), CEPH_NOSNAP)));
        t.write(cid, hoid, 0, 0x1000, a);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    Formatter *f = Formatter::create("store_test", "json-pretty", "json-pretty");
    store->generate_db_histogram(f);
    f->flush(cout);
    cout << std::endl;
}

TEST_P(KvsStoreTest, KVDBStatsTest) {
    if (string(GetParam()) != "bluestore")
        return;

    g_conf->set_val("rocksdb_perf", "true");
    g_conf->set_val("rocksdb_collect_compaction_stats", "true");
    g_conf->set_val("rocksdb_collect_extended_stats","true");
    g_conf->set_val("rocksdb_collect_memory_stats","true");
    g_ceph_context->_conf->apply_changes(NULL);
    int r = store->umount();
    ASSERT_EQ(r, 0);
    r = store->mount(); //to force rocksdb stats
    ASSERT_EQ(r, 0);

    ObjectStore::Sequencer osr("test");
    int NUM_OBJS = 200;
    coll_t cid;
    string base("testobj.");
    bufferlist a;
    bufferptr ap(0x1000);
    memset(ap.c_str(), 'a', 0x1000);
    a.append(ap);
    {
        ObjectStore::Transaction t;
        t.create_collection(cid, 0);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }
    for (int i = 0; i < NUM_OBJS; ++i) {
        ObjectStore::Transaction t;
        char buf[100];
        snprintf(buf, sizeof(buf), "%d", i);
        ghobject_t hoid(hobject_t(sobject_t(base + string(buf), CEPH_NOSNAP)));
        t.write(cid, hoid, 0, 0x1000, a);
        r = apply_transaction(store, &osr, std::move(t));
        ASSERT_EQ(r, 0);
    }

    Formatter *f = Formatter::create("store_test", "json-pretty", "json-pretty");
    store->get_db_statistics(f);
    f->flush(cout);
    cout << std::endl;
    g_conf->set_val("rocksdb_perf", "false");
    g_conf->set_val("rocksdb_collect_compaction_stats", "false");
    g_conf->set_val("rocksdb_collect_extended_stats","false");
    g_conf->set_val("rocksdb_collect_memory_stats","false");
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixSharding) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "4096", 0 }, // must be the first!
            { "num_ops", "50000", 0 },
            { "max_write", "65536", 0 },
            { "max_size", "262144", 0 },
            { "alignment", "4096", 0 },
            { "bluestore_max_blob_size", "65536", 0 },
            { "bluestore_extent_map_shard_min_size", "60", 0 },
            { "bluestore_extent_map_shard_max_size", "300", 0 },
            { "bluestore_extent_map_shard_target_size", "150", 0 },
            { "bluestore_default_buffered_read", "true", 0 },
            { "bluestore_default_buffered_write", "true", 0 },
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixCsumAlgorithm) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "65536", 0 }, // must be the first!
            { "max_write", "65536", 0 },
            { "max_size", "1048576", 0 },
            { "alignment", "16", 0 },
            { "bluestore_csum_type", "crc32c", "crc32c_16", "crc32c_8", "xxhash32",
                    "xxhash64", "none", 0 },
            { "bluestore_default_buffered_write", "false", 0 },
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixCsumVsCompression) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "4096", "16384", 0 }, //to be the first!
            { "max_write", "131072", 0 },
            { "max_size", "262144", 0 },
            { "alignment", "512", 0 },
            { "bluestore_compression_mode", "force", 0},
            { "bluestore_compression_algorithm", "snappy", "zlib", 0 },
            { "bluestore_csum_type", "crc32c", 0 },
            { "bluestore_default_buffered_read", "true", "false", 0 },
            { "bluestore_default_buffered_write", "true", "false", 0 },
            { "bluestore_sync_submit_transaction", "false", 0 },
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixCompression) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "4096", "65536", 0 }, // to be the first!
            { "max_write", "1048576", 0 },
            { "max_size", "4194304", 0 },
            { "alignment", "65536", 0 },
            { "bluestore_compression_mode", "force", "aggressive", "passive", "none", 0},
            { "bluestore_default_buffered_write", "false", 0 },
            { "bluestore_sync_submit_transaction", "true", 0 },
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixCompressionAlgorithm) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "4096", "65536", 0 }, // to be the first!
            { "max_write", "1048576", 0 },
            { "max_size", "4194304", 0 },
            { "alignment", "65536", 0 },
            { "bluestore_compression_algorithm", "zlib", "snappy", 0 },
            { "bluestore_compression_mode", "force", 0 },
            { "bluestore_default_buffered_write", "false", 0 },
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixNoCsum) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "4096", "65536", 0 }, // to be the first!
            { "max_write", "65536", 0 },
            { "max_size", "1048576", 0 },
            { "alignment", "512", 0 },
            { "bluestore_max_blob_size", "262144", 0 },
            { "bluestore_compression_mode", "force", "none", 0},
            { "bluestore_csum_type", "none", 0},
            { "bluestore_default_buffered_read", "true", "false", 0 },
            { "bluestore_default_buffered_write", "true", 0 },
            { "bluestore_sync_submit_transaction", "true", "false", 0 },
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}

TEST_P(KvsStoreTestSpecificAUSize, SyntheticMatrixPreferDeferred) {
    if (string(GetParam()) != "bluestore")
        return;

    const char *m[][10] = {
            { "bluestore_min_alloc_size", "4096", "65536", 0 }, // to be the first!
            { "max_write", "65536", 0 },
            { "max_size", "1048576", 0 },
            { "alignment", "512", 0 },
            { "bluestore_max_blob_size", "262144", 0 },
            { "bluestore_compression_mode", "force", "none", 0},
            { "bluestore_prefer_deferred_size", "32768", "0", 0},
            { 0 },
    };
    do_matrix(m, store, doSyntheticTest);
}
#endif

INSTANTIATE_TEST_CASE_P(
  ObjectStore,
  KvsStoreTest,
  ::testing::Values(
    "kvsstore"));

int main(int argc, char **argv){
  vector<const char*> args;
  argv_to_vec(argc, (const char **)argv, args);
  env_to_vec(args);
  auto cct = global_init(NULL, args, CEPH_ENTITY_TYPE_CLIENT,
			 CODE_ENVIRONMENT_UTILITY, 0);
  common_init_finish(g_ceph_context);	
  g_ceph_context->_conf->apply_changes(NULL);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
