--Usage: lua make_one.lua number_instances(>1)

function main()
    local n = arg[1] - 1
    for i = 0, n do
        local conf = "one/makoa_one_" .. i .. ".conf"
        local cont = "log_exporter = 0\nimport = tyra " .. (10000 + i) .. "\nname = makoa_one_" .. i .. "\nexport_threads = 1\npooling = 0\n"
		if(i ~= n) then
            cont = cont .. "export = viktor sight localhost:" .. (10001 + i) .. "\n"
        else
			cont = cont .. "export = ying $0\n"
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
            os.execute("chmod +x run_oneY.sh")
        end
    end
    sh = sh .. "./bitfinex one/alco_one.conf"
    io.open("run_one.sh", "w+") : write(sh) : close()
    os.execute("chmod +x run_one.sh")
end

main()

