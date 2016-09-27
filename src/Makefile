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



all: dids_client dids_server tester

ppm.o: ppm.c ppm.h
	cc -c -o ppm.o ppm.c `pkg-config --cflags --libs MagickWand`

ppm_compare.o: ppm_compare.c ppm.h

ppm_sql.o: ppm_sql.c ppm.h

ppm_info.o: ppm_info.c ppm.h

ppm_list.o: ppm_list.c ppm.h

ppm_dao.o: ppm_dao.c ppm.h

dids_server.o: dids_server.c ppm.h
	cc -c -o dids_server.o dids_server.c `pkg-config --cflags --libs MagickWand`

similar_but_different_dao.o: similar_but_different_dao.c ppm.h

dids_server: dids_server.o ppm_dao.o ppm.o ppm_sql.o ppm_info.o ppm_compare.o \
   similar_but_different_dao.o ppm_list.o ppm.h
	gcc -L/usr/lib/ -o dids_server ppm.o ppm_dao.o ppm_sql.o \
	    similar_but_different_dao.o ppm_info.o ppm_compare.o ppm_list.o dids_server.o \
	    -lpq `pkg-config --libs MagickWand` -lpthread
	# chcon -Rt httpd_sys_script_exec_t dids_server

tester: tester.c similar_but_different_dao.o ppm.o ppm_info.o ppm_list.o ppm_compare.o ppm_sql.o ppm_dao.o ppm.h
	gcc -L/usr/lib/ -o tester tester.c ppm.o ppm_info.o ppm_list.o \
	similar_but_different_dao.o ppm_compare.o ppm_sql.o ppm_dao.o \
	-lpq `pkg-config --cflags --libs MagickWand` -lpthread

clean:
	rm -f tester dids_server dids_client *.o

test: tester
	  ./tester "dbname = 'myTestDatabase' user = 'myTestSqlUser' connect_timeout = '10'" myTestImage.jpg

../dids_client: dids_client
	cp dids_client ../dids_client

../dids_server: dids_server
	cp dids_server ../dids_server

install: ../dids_client ../dids_server

