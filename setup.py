from distutils.core import setup, Extension
setup(name="freerdp", version="1.0",
      ext_modules=[
                   Extension("freerdp", 
                             sources=["src/freerdp.c", 
                                      "src/freerdp_py.c"],
                             include_dirs=["src",
                                           "sub_modules/FreeRDP/include", 
                                           "sub_modules/FreeRDP/winpr/include"],
                             libraries=[":libfreerdp-client.so.1.1.0", 
                                        ":libfreerdp-gdi.so.1.1.0",
                                        ":libfreerdp-utils.so.1.1.0", 
                                        ":libfreerdp-core.so.1.1.0", 
                                        ":libwinpr-synch.so.0.1.0"]
                   )
      ]
)

