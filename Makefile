#
# Make file for the image comparison daemon.
#
# Takes thumbnails of the images and stores them in SQL
# When requested the daemon will use the thumbnails to give a list of images
#   that are very similar.
# Control of the daemon is either by command line or dids_client

# Postgres header file
export C_INCLUDE_PATH=/usr/include/postgresql

# Useful commands to find where PG include and lib files are.
# pg_config --includedir
# pg_config --libdir
# -I$(pg_config --includedir) -L$(pg_config --libdir)

all: build/dids_client build/dids_server test/build/dids_server_image_test test/build/dids_list_test

build/dids_client: src/dids_client.c
	gcc -L/usr/lib/ -o build/dids_client src/dids_client.c

build/ppm.o: src/ppm.c src/dids.h
	cc -c -o build/ppm.o src/ppm.c `pkg-config --cflags --libs MagickWand`

build/ppm_compare.o: src/ppm_compare.c src/dids.h
	cc -c -o build/ppm_compare.o src/ppm_compare.c

build/ppm_sql.o: src/ppm_sql.c src/dids.h
	cc -c -o build/ppm_sql.o src/ppm_sql.c

build/ppm_info.o: src/ppm_info.c src/dids.h
	cc -c -o build/ppm_info.o src/ppm_info.c

build/ppm_list.o: src/ppm_list.c src/dids.h
	cc -c -o build/ppm_list.o src/ppm_list.c

build/ppm_dao.o: src/ppm_dao.c src/dids.h
	cc -c -o build/ppm_dao.o src/ppm_dao.c

build/dids_util.o: src/dids_util.c src/dids.h
	cc -c -o build/dids_util.o src/dids_util.c

build/dids_server.o: src/dids_server.c src/dids.h
	cc -c -o build/dids_server.o src/dids_server.c `pkg-config --cflags --libs MagickWand`

build/similar_but_different_dao.o: src/similar_but_different_dao.c src/dids.h
		cc -c -o build/similar_but_different_dao.o src/similar_but_different_dao.c

build/dids_server: build/dids_server.o build/ppm_dao.o build/ppm.o build/ppm_sql.o  \
   build/ppm_info.o build/ppm_compare.o build/similar_but_different_dao.o \
   build/ppm_list.o build/dids_util.o src/dids.h
	gcc -L/usr/lib/ -o build/dids_server build/ppm.o build/ppm_dao.o \
	    build/ppm_sql.o build/similar_but_different_dao.o build/ppm_info.o \
		build/ppm_compare.o build/ppm_list.o build/dids_server.o \
	    build/dids_util.o -lpq `pkg-config --libs MagickWand` -lpthread
	# chcon -Rt httpd_sys_script_exec_t build/dids_server

test/build/dids_server_image_test: test/dids_server_image_test.c build/similar_but_different_dao.o \
	build/ppm.o build/ppm_info.o build/ppm_list.o build/ppm_compare.o build/ppm_sql.o \
	build/ppm_dao.o src/dids.h
	gcc -L/usr/lib/ -o test/build/dids_server_image_test test/dids_server_image_test.c build/ppm.o \
	build/ppm_info.o build/ppm_list.o build/similar_but_different_dao.o build/ppm_compare.o \
	build/ppm_sql.o build/ppm_dao.o build/dids_util.o \
	-lpq `pkg-config --cflags --libs MagickWand` -lpthread

test/build/dids_list_test: test/dids_list_test.c build/ppm_list.o build/ppm_info.o \
    build/dids_util.o src/dids.h
	gcc -L/usr/lib/ -o test/build/dids_list_test test/dids_list_test.c build/ppm_list.o \
	build/ppm_info.o

test/build/.test_db_setup:
	test/postgres_setup_test_database.sh
	touch test/build/.test_db_setup

test: test/build/dids_list_test test/build/.test_db_setup test/build/dids_server_image_test
	test/build/dids_list_test
	test/build/dids_server_image_test "dbname = 'test' user = 'test' connect_timeout = '10'" test/resources/image.jpg

clean:
	rm -f build/dids_server build/dids_client test/build/dids_server_image_test test/build/dids_list_test build/*.o test/build/*.o

../../bin/dids_client: build/dids_client
	cp build/dids_client ../../bin/dids_client

../../bin/dids_server: build/dids_server
	cp build/dids_server ../../bin/dids_server

install: ../../bin/dids_client ../../bin/dids_server

