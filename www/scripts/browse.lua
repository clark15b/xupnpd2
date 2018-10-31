http:content_type('text/html')

http:out('<html><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8"/><title>Browse</title></head>\n<body>\n')

objid=tonumber(args['id']) or 0

page=tonumber(args['p']) or 1

if page<1 then page=1 end

page_size=15

total,t=browse(objid,page,page_size)

base=(page-1)*page_size

for i,j in ipairs(t) do
    if tonumber(j.objtype)==0 then
        http:out(string.format('%d. <a href="%s?id=%s">%s</a><br>\n',base+i,REQUEST_URI,j.objid,j.name))
    else
        if tonumber(j.mimecode)~=32 then
            http:out(string.format('%d. %s<br>\n',base+i,j.name))
        else
            http:out(string.format('%d. <a href="play.lua?id=%s">%s</a><br>\n',base+i,j.objid,j.name))
        end
    end
end

http:out('<br>\n')

http:out(string.format('<a href="%s?id=%d&p=1">first</a> | ',REQUEST_URI,objid))

if page>1 then
    http:out(string.format('<a href="%s?id=%d&p=%d">prev</a>',REQUEST_URI,objid,page-1))
else
    http:out('prev');
end

http:out(' | ')

if total>page*page_size then
    http:out(string.format('<a href="%s?id=%d&p=%d">next</a>',REQUEST_URI,objid,page+1))
else
    http:out('next');
end

http:out('<br>\n')

http:out(string.format('<a href="%s?id=0">top</a> | <a href="/index.html">home</a>\n',REQUEST_URI))

http:out('</body></html>\n')
