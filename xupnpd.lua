function click2stream_translate_url(url,method)
    local stream_id=string.match(utils.fetch(url),"new%s+Angelcam.player%('player',%s*{%s*id:%s*'(.-)'")

    local req='currentTime=' .. os.date('!%Y-%m-%dT%H%%3A%M%%3A%S.000Z') .. '&offset=-180&dst=0&host=' .. string.match(url,"http://(.+)$") .. '&rtsp=0&sdk=1&streamType=hls&hash_id=' .. stream_id

    local resp=utils.fetch("http://v.angelcam.com/v1/player/" .. stream_id,req)

    local s=string.match(resp,'"streamUrl":"(.-m3u8.-)"')

    local result=string.gsub(s,'\\/','/')

    return result
end
