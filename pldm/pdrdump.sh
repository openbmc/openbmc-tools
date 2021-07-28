#!/bin/sh
timestamp=date '+%s'
pdrtmp_file="/tmp/pdrdump.tmp"
pdrdump_file="/tmp/pdrdump$timestamp.txt"
printf "Dumping PDRs to $pdrdump_file ..."
i=0
done=0
while [ $done -eq 0 ]
do
    pldmtool platform GetPDR -d $i > $pdrtmp_file
    echo "pldmtool platform GetPDR -d $i" >> $pdrdump_file
    cat $pdrtmp_file >> $pdrdump_file
    echo "" >> $pdrdump_file
    i=`grep nextRecordHandle $pdrtmp_file | awk '{print $2}'`
    i=${i::-1}
    if [ $i -eq 0 ]; then done=1; fi
done
printf " Complete!\n"
rm $pdrtmp_file