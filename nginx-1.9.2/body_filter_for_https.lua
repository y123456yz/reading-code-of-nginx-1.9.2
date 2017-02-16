--access_by_lua 'ngx.ctx.uri_args = ngx.req.get_uri_args() ngx.ctx.useragent_convert_str=ngx.req.get_headers()["User-Agent"] ngx.ctx.host_convert_str=ngx.req.get_headers()["Host"]';
--body_filter_by_lua_file /home/yyz/tengine/body_filter_for_https.lua;

local function find_namedstr_in_str_table(str_table, named_str)
    if str_table == nil or named_str == nil then
       return 0
    end
    
    local ua = string.lower(named_str)
 
    for _, s in ipairs(str_table) do
         find_result = string.find(ua, s, 1, true)
         if find_result then
             return 1
         end
    end
    
    return 0
end

--0:delete   1:http to https  2:not change           
local function get_ngx_res_useragent_result(useragent_str)
    if(useragent_str == nil) then
        return 0
    end
	
	local ua = string.lower(useragent_str)
	
	local is_testxxxtest_app = find_namedstr_in_str_table({"testxxxtest", "xxx2", "xxx3"}, ua)
	local is_xxx3 = string.find(ua, 'xxx3', 1, true)
	local is_xxx2 = string.find(ua, 'xxx2', 1, true)
	
	if is_testxxxtest_app == 1 and is_xxx3 then
		return 1
	elseif is_testxxxtest_app == 1 and is_xxx2 then
		return 2
	else
		return 0
	end

end

local function ngx_resp_https_convert(http_str, convert_result)
   
   if http_str==nil then
       return
   end

   local low_s = string.lower(http_str)
   if low_s == nil then
       return
   end

   local is_testxxxtest_hostname = find_namedstr_in_str_table({"xxx.com"}, low_s)
    
   if is_testxxxtest_hostname==1 and convert_result==0 then
       return string.gsub(low_s, "http%w?://", "//")
   elseif is_testxxxtest_hostname==1 and convert_result==1 then
       return string.gsub(low_s, "http%w?://", "https://")
   end
end

local function ngx_resp_body_convert(body, convert_result)
	if(body) then
           a, n=string.gsub(body, "http://.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?.?", 
                function (s)
                    return ngx_resp_https_convert(s, convert_result)
                end
           ) 
          return a
      end 
end

local function ngx_check_host(host_list, host_str)
  if host_list == nil or host_str == nil then
     return 0
  end
 
  local low_host = string.lower(host_str)
  local is_match = find_namedstr_in_str_table(host_list, low_host)

  if is_match==1 then
      return 1
  else
      return 0
  end
end

local function ngx_check_uri_args(uri_args_list, args_str)
  local is_match
  for key, val in pairs(args_str) do
      local arg_kv = ""
      if type(val) == "table" then
          arg_kv=arg_kv..key.."="..table.concat(val, ", ")
      else
          arg_kv=arg_kv..key.."="..val
      end

      local is_match=find_namedstr_in_str_table(uri_args_list, arg_kv)
      if is_match==1 then
          return arg_kv
      end
  end

  return nil
end

if ngx.arg[1] == nil then
    return
end

local cdn_host_match
local notcdn_host_match
local convert_result
local uri_arg_kv

if ngx.ctx.host_convert_str then
    local cdn_host_list={"xxx3.com"}
    local notcdn_host_list={"xxx1.com", "xxx2.com"}
    cdn_host_match = ngx_check_host(cdn_host_list, ngx.ctx.host_convert_str)
    if cdn_host_match == 0 then
       notcdn_host_match = ngx_check_host(notcdn_host_list, ngx.ctx.host_convert_str)
    end

    if cdn_host_match == 0 and notcdn_host_match == 0 then
       return 
    end
end

if ngx.ctx.uri_args then
    local uri_args_list = {"os=xxx1", "os=xxx2", "os=xxx3"}
    uri_arg_kv = ngx_check_uri_args(uri_args_list, ngx.ctx.uri_args)
end


--0:delete   1:http to https  2:not change
if ngx.ctx.useragent_convert_str and notcdn_host_match==1 then
    convert_result = get_ngx_res_useragent_result(ngx.ctx.useragent_convert_str)
end


if cdn_host_match == 1 and uri_arg_kv == "os=xxx1" then
   convert_result=0
end

if cdn_host_match == 1 and uri_arg_kv == "os=xxx2" then
   convert_result=1
end

if cdn_host_match == 1 and uri_arg_kv == "os=xxx3" then
   convert_result=2
end

ngx.log(ngx.ERR, "dump convert result--------- convert_result:",  convert_result, " uri_arg_kv:", uri_arg_kv, " cdn_host_match:", cdn_host_match, " notcdn_host_match:", notcdn_host_match)
   
ngx.arg[1]=ngx_resp_body_convert(ngx.arg[1], convert_result)


