#!/bin/bash

# Series System
# This script is run inside the guest VM to setup series on the guest VM.

set -o xtrace
set -o errexit

# Instance name. Multiple instances can be run on same server.
# Must be just a single word.
INSTANCE=${INSTANCE:-test}

# Database user
DB_USER=${DB_USER:-${INSTANCE}}

# Username who admins postgres.
POSTGRES_ADMIN=${POSTGRES_ADMIN:-'postgres'}

# Show settings
echo "Settings"
echo "INSTANCE='${INSTANCE}'"
echo "POSTGRES_ADMIN='${POSTGRES_ADMIN}'"
echo "DB_USER='${DB_USER}'"
######### Postgres Section #############
# Create postgres user for this instance
sudo su - "${POSTGRES_ADMIN}" -c "createuser ${DB_USER}"

# Create postgres database for this instance
sudo su - "${POSTGRES_ADMIN}" -c "createdb --owner ${DB_USER} ${INSTANCE}"

# Ask postgres where its pg_hba.conf file is. May require postgres to be running.
PGCONF=$(su - "${POSTGRES_ADMIN}" -c "psql -t -P format=unaligned -c 'show hba_file';")

# Add Postgres permission for Unix socket.
echo "Postgres config is here: ${PGCONF}"
sudo sed "/Put your actual configuration here/a# -- start of ${INSTANCE} config\n# 'local' is for Unix domain socket connections only\n#       Database    Role\nlocal   ${INSTANCE}      ${DB_USER}                               trust\n# -- end of ${INSTANCE} config\n" -i.bak "${PGCONF}"

# Get Postgres to re-read the config
sudo service postgresql restart

cat << SQLSCHEMA | su - "${POSTGRES_ADMIN}" -c "psql ${INSTANCE}"
BEGIN;
CREATE SEQUENCE public.dids_ppm_id_seq
    START WITH 1 INCREMENT BY 1 NO MINVALUE NO MAXVALUE CACHE 1;
ALTER TABLE public.dids_ppm_id_seq OWNER TO test;
CREATE TABLE public.dids_ppm (
    id integer DEFAULT nextval('public.dids_ppm_id_seq'::regclass) NOT NULL,
    width integer NOT NULL,
    height integer NOT NULL,
    hexdata text NOT NULL,
    created timestamp without time zone DEFAULT now() NOT NULL,
    modified timestamp without time zone DEFAULT now() NOT NULL,
    external_ref character varying(120) NOT NULL
);
ALTER TABLE public.dids_ppm OWNER TO test;
COMMIT;
SQLSCHEMA
