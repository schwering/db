# First step: phpize
phpize || (echo "phpize failed. I don't know why. Maybe the php-dev package isn't installed?" && read)

# Second step: configure
./configure --enable-dingsbums || (echo "configure failed. I don't know why." && read)

# Third step: set LDFLAGS and CFLAGS variables (include directory and 
# linking directory ../db and linking ../db/libdb.a)
(cat Makefile | sed s/^'LDFLAGS ='.*$/'LDFLAGS = -L..\/db -ldb'/ | sed s/^'CFLAGS = '.*$/'CFLAGS = -I..\/db'/ >Makefilenew) || (echo "Modifying the LDFLAGS and CFLAGS of the Makefile failed." && read)
mv Makefilenew Makefile || (echo "Replacing the generated Makefile failed." && read)

# Compile
make || (echo "make failed. Have you already build ''libdb.a''? If not, cd to ../db and type make." && read)

# Get root password
echo -n "Enter root password (for sudo): "
stty -echo
read rootpw
stty echo
echo ""

# Update php.ini files
phpini_path=/etc/
echo -n "Enter search path for php.inis ($phpini_path): "
read tmp_path
if [ "$tmp_path" != "" ]; then
	phpini_path=$tmp_path
fi
phpinis=`echo $rootpw | sudo -S find $phpini_path -name php.ini`
for ini in $phpinis
do
	echo -n "Update $ini [y/N]? "
	read tmp
	if [ "$tmp" = "y" ]; then
		echo $rootpw | sudo -S sh -c "echo extension=dingsbums.so >> $ini" &&\
		echo "Added ''extension=dingsbums.so'' entry to $ini." ||\
		echo "Adding the ''extension=dingsbums.so'' entry to $ini failed. Try by hand. I'll go on..."
	fi
done

# Copy dingsbums.so
extension_path=/usr/lib/php5/200?????*/
echo -n "Enter PHP extension path ($extension_path): "
read tmp_path
if [ $tmp_path != "" ]; then
	extension_path=$tmp_path
fi
echo $rootpw | sudo -S cp modules/dingsbums.so $extension_path &&\
echo "Copied dingsbums.so to $extension_path." ||\
echo "Copying dingsbums.so to $extension_path failed."

