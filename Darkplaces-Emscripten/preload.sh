if [ -z "$1" ]
then
echo "This script loads a directory into an output data file and generates file.js that the server needs to load those files"
echo "Usage: ./preload.sh directory output.data"
exit 1
fi
python3 $EMSDK/upstream/emscripten/tools/file_packager.py $2 --preload $1@/ --lz4 --js-output=files.js
