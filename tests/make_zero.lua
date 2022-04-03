--Usage: lua make_zero.lua number_instances(>1)

function main()
    local n = arg[1] - 1
    for i = 0, n do
        local conf = "zero/makoa_zero_" .. i .. ".conf"
        local cont = "log_exporter = 0\nimport = tyra " .. (20000 + i) .. "\nname = makoa_zero_" .. i .. "\nexport_threads = 4\npooling = 0\n"
        if(i == 0) then
            for i = 1, n do
                cont = cont .. "export = viktor sight localhost:" .. (20000 + i) .. "\n"
            end
        elseif(i ~= n) then
        else
			cont = cont .. "export = ying $0\n"
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
            os.execute("chmod +x run_zeroY.sh")
        end
    end
    sh = sh .. "./bitfinex zero/alco_zero.conf"
    io.open("run_zero.sh", "w+") : write(sh) : close()
    os.execute("chmod +x run_zero.sh")
end

main()

