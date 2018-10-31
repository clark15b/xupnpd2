--[[

CGI standart global variables:
DOCUMENT_ROOT (or G['DOCUMENT_ROOT'])
REMOTE_ADDR
REQUEST_METHOD
REQUEST_URI
SCRIPT_FILENAME
SERVER_NAME
SERVER_PORT

HTTP_*                                          -- request headers ('HTTP_USER_AGENT' for example)

args['arg1']                                    -- request line arguments (for example: '/example.lua?arg1=value1&arg2=value2')

http:data()                                     -- get POST body if exist
http:data_length()                              -- get POST body length

http:status(200)                                -- set http status code
http:content_type('text/plain')                 -- set response Content-Type header and http status code to 200
http:add_header('X-Text: test')                 -- append extra headers to response
http:out('hello world')                         -- response body
print('Hello world')                            -- to log

--]]

http:content_type('text/html')

http:out('<html><head><title>example</title></head>\n<body>\n');

http:out('<h1>Hello world</h1>\n<h2>'..SERVER_NAME..':'..SERVER_PORT..'</h2><h2>'..(HTTP_USER_AGENT or '')..'</h2>\n')

for i,j in pairs(args) do
    http:out(i,j,'<br>\n')
end

http:out('</body></html>\n')