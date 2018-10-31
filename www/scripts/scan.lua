http:content_type('text/html')

scan_for_media()

http:out('<html><head><META HTTP-EQUIV="Refresh" content="0; url=http://'..SERVER_NAME..':'..SERVER_PORT..'/"></head>\n<body></body></html>\n')
