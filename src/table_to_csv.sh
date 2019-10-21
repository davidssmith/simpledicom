#!/bin/sh

cat part06.html | awk -f extract_standard_file_meta.awk |
grep -i -e '<a\|</\?tbody\|</\?td\|</\?tr\|</\?th' |
sed 's/^[\ \t]*//g' |
tr -d '\n' |
sed 's/<\/tr[^>]*>/\n/Ig'| 
sed 's/^[^/]*<\/a>//'  | 
sed 's/<\/p><\/td>[^/]*\/a>/,/Ig' |
sed 's/<\/p>.*/,/I' |
sed 's/<.*//I' |
sed 's/[()]//g'
