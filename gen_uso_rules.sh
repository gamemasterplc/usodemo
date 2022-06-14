#!/bin/bash

print_uso_subdirs() {
	#Loop through directories recursively
    for dir in "$1"/*;do
        if [ -d "$dir" ];then
		#Print directory and separate wtih space
            echo -n "uso_src/$dir "
            print_uso_subdirs "$dir"
        fi
    done
}

print_uso_rules() {
    for i in *;do
        if [ -d "$i" ];then
			#Add main USO directory
            echo -n ""$i"_SRCDIRS := uso_src/"$i" "
			#Add USO subdirectories
			print_uso_subdirs "$i"
			#Add newline
			echo ""
			#Add to USO directories
			echo "USO_SRCDIRS += \$("$i"_SRCDIRS)"
			#Get source files from directories
			echo ""$i"_CFILES := \$(foreach dir,\$("$i"_SRCDIRS),\$(wildcard \$(dir)/*.c))"
			echo ""$i"_CXXFILES := \$(foreach dir,\$("$i"_SRCDIRS),\$(wildcard \$(dir)/*.cpp))"
			echo ""$i"_SFILES := \$(foreach dir,\$("$i"_SRCDIRS),\$(wildcard \$(dir)/*.s))"
			
			#Get objects from source files
			echo ""$i"_OBJECTS := \$(foreach file,\$("$i"_CFILES),\$(BUILD_DIR)/\$(file:.c=.o)) \\"
			echo "	\$(foreach file,\$("$i"_CXXFILES),\$(BUILD_DIR)/\$(file:.cpp=.o)) \\"
			echo "	\$(foreach file,\$("$i"_SFILES),\$(BUILD_DIR)/\$(file:.s=.o))"
			#Append to USO objects
			echo "USO_OBJECTS += \$("$i"_OBJECTS)"
			#Add to USO list
			echo "\$(USO_ELF_DIR)/"$i".elf: \$("$i"_OBJECTS)"
			echo "USO_NAMES += "$i""
        elif [ -f "$i" ]; then
			tmp_ext=${i#*.}
			#Add only c, cpp, and s files
			if [ $tmp_ext == "c" ] || [ $tmp_ext == "cpp" ] || [ $tmp_ext == "s" ]; then
				tmp_basename=${i%%.*}
				
				#Get object for source file
				echo ""$tmp_basename"_OBJECTS := \$(BUILD_DIR)/uso_src/"$tmp_basename".o"
				#Append to USO objects
				echo "USO_OBJECTS += \$("$tmp_basename"_OBJECTS)"
				#Add to USO list
				echo "\$(USO_ELF_DIR)/"$tmp_basename".elf: \$("$tmp_basename"_OBJECTS)"
				echo "USO_NAMES += "$tmp_basename""
			fi
        fi
    done
}

echo "USO_SRCDIRS += uso_src"
cd uso_src
print_uso_rules
cd ..