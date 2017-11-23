#! /bin/bash 

debug=1

execute ()
{
    $* >/dev/null
        if [ $? -ne 0 ]; then
        echo
        echo "ERROR: executing $*"
        echo
        exit 1
    fi
}

#performance test ...
performance_test_cacheoff()
{
echo "[info]============================>start performance test boot sequence(buf cache off)"
if [ $debug -eq 1 ]; then
    /bin/mmc_test/scripts/customer_test/hisense_boot.sh | tee -a /log/hisense.log
else
   /bin/mmc_test/scripts/customer_test/hisense_boot.sh | tee -a /log/hisense.log
   /bin/mmc_test/scripts/customer_test/lg_boot.sh | tee -a /log/lg.log
   /bin/mmc_test/scripts/customer_test/samsung_android_boot.sh | tee -a /log/samsung_android.log
   /bin/mmc_test/scripts/customer_test/samsung_dtv_boot.sh | tee -a /log/samsung_dtv.log
   /bin/mmc_test/scripts/customer_test/xiaomi_boot.sh | tee -a /log/xiaomi.log
fi
}

performance_test_cacheon()
{
echo "[info]============================>start performance test(buf cache on)"
../customer_test/hisense_boot.sh
../customer_test/lg_boot.sh
../customer_test/samsung_android_boot.sh
../customer_test/samsung_dtv_boot.sh
../customer_test/xiaomi_boot.sh

}



performance_test_cacheoff
#performance_test_cacheon


echo " ======================================= dd test completed! ======================================= "  








