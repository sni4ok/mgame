--Usage: lua make_one.lua number_instances(>1)

function main()
    local n = arg[1] - 1
    for i = 0, n do
        local conf = "one/makoa_one_" .. i .. ".conf"
        local cont = "log_exporter = 0\nport = " .. (10000 + i) .. "\nname = makoa_one_" .. i
		if(i ~= n) then
            cont = cont .. "\n\nexport = viktor sight localhost:" .. (10001 + i) .. "\n"
        else
			cont = cont .. "\n\n#BTCUSD with empty feed\nexport = ying 3842312473 50\n"
		end

        io.open(conf, "w+") : write(cont) : close()
    end
    local sh = ""
    for i = n, 0, -1 do
        local conf = "one/makoa_one_" .. i .. ".conf"
        if(i ~= n) then
            sh = sh .. "./makoa_server " .. conf .. " &\nsleep 0.1\n"
        else
            io.open("run_oneY.sh", "w+") : write("./makoa_server " .. conf .. "\n") : close()
        end
    end
    sh = sh .. "./bitfinex one/alco_one.conf"
    io.open("run_one.sh", "w+") : write(sh) : close()
end

main()

