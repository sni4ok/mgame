--Usage: lua make_zero.lua number_instances(>1)

function main()
    local n = arg[1] - 1
    for i = 0, n do
        local conf = "zero/makoa_zero_" .. i .. ".conf"
        local cont = "log_exporter = 0\nport = " .. (20000 + i) .. "\nname = makoa_zero_" .. i .. "\n"
        if(i == 0) then
            for i = 1, n do
                cont = cont .. "export = viktor sight localhost:" .. (20000 + i) .. "\n"
            end
        elseif(i ~= n) then
        else
			cont = cont .. "\n#BTCUSD with empty feed\nexport = ying 3842312473 50\n"
		end

        io.open(conf, "w+") : write(cont) : close()
    end
    local sh = ""
    for i = n, 0, -1 do
        local conf = "zero/makoa_zero_" .. i .. ".conf"
        if(i ~= n) then
            sh = sh .. "./makoa_server " .. conf .. " &\nsleep 0.1\n"
        else
            io.open("run_zeroY.sh", "w+") : write("./makoa_server " .. conf .. "\n") : close()
        end
    end
    sh = sh .. "./bitfinex zero/alco_zero.conf"
    io.open("run_zero.sh", "w+") : write(sh) : close()
end

main()

