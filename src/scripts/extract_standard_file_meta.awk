BEGIN {
	flag = 0
}

/Table 6-1. Registry of DICOM Data Elements/ {
#/Table A-1. UID Values/ {
#/Table 7-1. Registry of DICOM File Meta Elements/ {
	flag = 1
}

/tbody/ && flag == 1 {
	flag = 2
}

flag == 2 {
	print
}

/\/tbody/ && flag = 2 {
	flag = 3
}
