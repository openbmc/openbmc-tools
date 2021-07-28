#!/bin/sh
pdrdump_file=$1
printf "Dumping PDRs to $pdrdump_file ..."
i=0
done=0
while [ $done -eq 0 ]
do
    echo "pldmtool platform GetPDR -d $i" >> $pdrdump_file
    pldmtool platform GetPDR -d $i >> $pdrdump_file
    i=`grep nextRecordHandle $pdrdump_file | tail -n1 | awk '{print $2}'`
    i=${i::-1}
    if [ $i -eq 0 ]; then done=1; fi
done
printf " Complete!\n"