
                    DIDS - Duplicate Image Detection System

Preface:
Sorry I haven't had time to edit this. Various other documents and notes have
been brought together, but there may be some (ahem) duplicate information.

Welcome to DIDS.

As its name suggests, this software is used to find possible duplicates in images.

DIDS is multi-threaded, forking and has several optimisation techniques built in.
DIDS listens for up to a 100 client connections over the network, either IPv4 or IPv6.
The software is designed to be a helper application, that is providing image comparision
services to another application.

All the smarts and database interactions happen in the server.
The client is used to add images to the server and request possible duplicates.


DIDS requires only the libraries Image Magick and PostgreSQL.
The SQL code is very generic and should be very easy to switch to a different database.

To allow much faster image comparison, a thumbnail image is used rather than the full sized image.
This thumbnail is stored in the database when image files are added to the system.

Images are registered in DIDS with a external system reference string.
This could be an MD5 string of the original file, or a database ID in the external system.
The advantage of an MD5 string is it can be regenerated from just the file contents,
and remains the same even if the external database is lost and needs to be rebuilt.

There is also the concept of relationships between files. Files may be highly similar,
but when reviewed, the files may be in fact different.  In DIDS these files may be marked
as 'similar but different' and then DIDS will never again report these as possible duplicates.


DIDS produces a list of likely image duplicates.
DIDS is designed to supplement other software to help identify likely duplicate images.
Typically the external software will pass each image to DIDS, along with the image's reference ID within
the external system. Once an image is added to DIDS, DIDS has no further need of the original image.

The "Similar But Different" Concept
In the external system the humans work through the list of possible duplicates.
It is possible for two different images to be reported as possible duplicates if they are very similar.
If the files are indeed different then the relationship needs to be recorded in the 'similar_but_different'
table/view.

That way the next time DIDS looks for duplicates it will ignore the 'similar_but_different' cases 
from previous runs.

Terminology:
  DIDS          : Duplicate Image Detection System. This software.
  external_ref  : For each file in an external system there is a string called 'external_ref'.
         When possible duplicates are reported, the external_ref strings are reported.
         The MD5 sum is a good choice as it won't change and can be easily regenerated.
         The ID in the external application could be used, but DIDS will need to be told
           if the ID changes.
  full compare  : All image (thumbnails) within DIDS are compared to each other.
  quick compare : An exteral image is compared to all image (thumbnails) in DIDS.


=====================================================================================
Database Requirements:

Internally (within DIDS):

DIDS stores a list of PPMs, which are basically thumbnail images, together
with a external_ref string used by the external system.

Externally (outside DIDS):

 DIDS requires the external system maintain a 'dids_similar_but_different'
   table (or view).

 NOTE: Please note the constraint. This gives performance improvements and is required.

 Option 1: TABLE
 External system explicitly maintains a table of similar_but_different.

 CREATE TABLE dids_similar_but_different (
    external_ref varchar(20),
    external_ref_other varchar(20),
    constraint external_ref_order check ( external_ref < external_ref_other));
 Note: choose whatever length of varchar you need.

 Option 2: VIEW
 External system may already have a table that holds relationships, so a view
 to that table may be enough.

 CREATE  VIEW  dids_similar_but_different
 AS

 SELECT fingerprint as external_ref, fingerprint_other as external_ref_other
 FROM direntryrelationship
 WHERE type = 'similar_but_different' AND fingerprint < fingerprint_other

 UNION

 SELECT fingerprint_other as external_ref, fingerprint as external_ref_other
 FROM direntryrelationship
 WHERE type = 'similar_but_different' AND fingerprint > fingerprint_other;

=====================================================================================

Comparison process:

Quick Compare:

A single image file is compared to all image (thumbnails) within DIDS.
DIDS will fork a client for each quick compare request, but remain 
single threaded.
DIDS will return the external_ref strings of potentual duplicate images.

Full Compare:
All the image (thumbnails) within DIDS are compared with each other.
DIDS will fork a client for each full compare request, and will become
multi threaded to make use of multiple CPUs should they be present.
DIDS will return the external_ref strings of potentual duplicate images.

Some notes about the FULL Comparison process.

For N images, the number of comparisons is:  N * ( N - 1) / 2.
As shown in the following diagram:

N
4|- - - -
3|- - - *
2|- - * *
1|- * * *
__________
X|1 2 3 4 N


Outline of the Image Comparison process:
* Consider a set of n images.
* Each image must be compared to every other image, but only once.
* Consider an n*n grid and comparing images (i,j) where i<j.
  Note the triangle that is formed.
* There are n*(n-1)/2 image comparisons that need to be made.
* To permit concurrent processing, the triangle is split into 'work units'.
* Each work unit is a horizontal stripe of the triangle.
* Each work unit is to compare the first image to each of the remaining images in the work unit (stripe).
* There are n-1 work units in total, each 1 image than the last work unit.




For full comparisons DIDS will launch a child 

==============================================================================================
Reducing False Positives:
* When false positives are found, they are recorded as 'similar but different' entries in a table.
* When processing possible duplicate images, remove any previously recorded duplicates.
* The 'similar but different' entries are linked to the original files by 'fingerprint' values.
* Fingerprints don't change because they are based only on the contents of the file, not the file id.


* Order the PPMs external_ref by dictionary sort of fingerprint.
This allows the sorting algorithm to be optimised to reduce effort to find potential duplicates.


