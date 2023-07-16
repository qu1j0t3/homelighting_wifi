## TODO: Find the CMake way to do this

index.c : index.html
	echo "char *index_html=\"\\" > $@
	sed -e 's/"/\\"/g' -e 's/$$/\\/' $< >> $@
	echo "\";" >> $@