#

echo "Running 'buildCSM' on all .csm files"
echo " "
if exist windoze.log del windoze.log

for %%f IN (..\data\basic\*.csm ..\data\*.csm) DO buildCSM -noviz %%f >> windoze.log

echo "Completed in windoze.log"
