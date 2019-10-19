
BEGIN {
	FS=","
	print "#pragma once"
	print "struct Tag { uint32_t val; char *VR; char *keyword; char VM[4];};"
	print "static struct Tag dict[] = {"
}

{
#	if ($3 ~ /or/) {
#		split($3, v, " or ")
#		print "{0x" $1 ",0x" $2 ",\"" v[1] "\",\"" $4 "\"},"
#		print "{0x" $1 ",0x" $2 ",\"" v[2] "\",\"" $4 "\"},"
#	} else
		print "{0x" $1 $2 ",\"" $3 "\",\"" $4 "\",\"" $5 "\"},"
}


END {
	print "};"
	print "static int DICTSIZE = " NR ";"
}
