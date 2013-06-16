ext
===

This directory holds git submodules that point to libraries needed by the WebM and WebP plug-ins.

You will need to manually add the following to this directory because the owners don't have a git repository I can embed:

* [Premiere CS5 SDK](http://www.adobe.com/devnet/premiere/sdk/cs5.html)
* [Photoshop CS5 SDK](http://www.adobe.com/devnet/photoshop/sdk.html)
* [libogg](http://www.xiph.org/downloads/)
* [libvorbis](http://www.xiph.org/downloads/)
 
If the submodule contents are missing, you should be able to get them by typing:

`git submodule init`
`git submodule update`