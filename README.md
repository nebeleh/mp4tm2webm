mp4tm2webm
==========

converts old timemachines

2013-06 - Saman

- reads old video tree, appends black frames to mp4 files and creates a new video tree.
- reads old video tree, converts them to webm and creates a new video tree.

requires ffmpeg and vpxenc, accessible from binary folder

$ ./converter sourcevideos_path destvideos_path_mp4 desvideos_path_webm targetBitsPerPixel

a targetBitsPerPixel of around 0.5 seems reasonable.