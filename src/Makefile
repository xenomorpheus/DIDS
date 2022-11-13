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



all: dids_client dids_server dids_server_image_test dids_list_test

ppm.o: ppm.c dids.h
	cc -c -o ppm.o ppm.c `pkg-config --cflags --libs MagickWand`

ppm_compare.o: ppm_compare.c dids.h

ppm_sql.o: ppm_sql.c dids.h

ppm_info.o: ppm_info.c dids.h

ppm_list.o: ppm_list.c dids.h

ppm_dao.o: ppm_dao.c dids.h

dids_util.o: dids_util.c dids.h

dids_server.o: dids_server.c dids.h
	cc -c -o dids_server.o dids_server.c `pkg-config --cflags --libs MagickWand`

similar_but_different_dao.o: similar_but_different_dao.c dids.h

dids_server: dids_server.o ppm_dao.o ppm.o ppm_sql.o ppm_info.o ppm_compare.o \
   similar_but_different_dao.o ppm_list.o dids_util.o dids.h
	gcc -L/usr/lib/ -o dids_server ppm.o ppm_dao.o ppm_sql.o \
	    similar_but_different_dao.o ppm_info.o ppm_compare.o ppm_list.o dids_server.o \
	    dids_util.o -lpq `pkg-config --libs MagickWand` -lpthread
	# chcon -Rt httpd_sys_script_exec_t dids_server

dids_server_image_test: dids_server_image_test.c similar_but_different_dao.o ppm.o ppm_info.o ppm_list.o ppm_compare.o ppm_sql.o ppm_dao.o dids.h
	gcc -L/usr/lib/ -o dids_server_image_test dids_server_image_test.c ppm.o ppm_info.o ppm_list.o \
	similar_but_different_dao.o ppm_compare.o ppm_sql.o ppm_dao.o dids_util.o \
	-lpq `pkg-config --cflags --libs MagickWand` -lpthread

dids_list_test: dids_list_test.c ppm_list.o ppm_info.o dids_util.o dids.h
	gcc -L/usr/lib/ -o dids_list_test dids_list_test.c ppm_list.o ppm_info.o

.test_db_setup:
	./postgres_setup_test_database.sh
	touch .test_db_setup

test: dids_list_test dids_server_image_test .test_db_setup
	./dids_list_test
	./dids_server_image_test "dbname = 'test' user = 'test' connect_timeout = '10'" resources/image.jpg

clean:
	rm -f dids_server dids_client dids_server_image_test dids_list_test *.o

../../bin/dids_client: dids_client
	cp dids_client ../../bin/dids_client

../../bin/dids_server: dids_server
	cp dids_server ../../bin/dids_server

install: ../../bin/dids_client ../../bin/dids_server

