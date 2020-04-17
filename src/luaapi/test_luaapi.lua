local wait = require('posix.sys.wait')
local unistd = require('posix.unistd')
local luast = require('libluast')

local strfmt = string.format


local shm_fn        = 'st_test_luaapi_shm'
local MIN_INT_VALUE = -2147483648
local MAX_INT_VALUE = 2147483647


local function test_luast_module_init()
    print('Test Start: ' ..debug.getinfo(1, "n").name)
    assert(type(luast) == 'table', 'module is not table type')

    -- direct require name
    assert(package.loaded.libluast,
           'module is not cached as libluast')

    -- module name registered in C function
    assert(package.loaded.luast,
           'module is not cached as luast')

    local module_protype = {
        module_init          = 'function',
        worker_init          = 'function',
        destroy              = 'function',
        get_registry         = 'function',
        new                  = 'function',
        ipairs               = 'function',
        pairs                = 'function',
        gc                   = 'function',
        proc_crash_detection = 'function',
    }

    assert(#module_protype, #luast, "wrong fields number")

    for key, value in pairs(luast) do
        local vtype = type(value)
        local etype = tostring(module_protype[key])
        assert(vtype == etype,
               key .. ' must be ' .. etype .. ', got ' .. vtype)
    end

    luast.module_init(shm_fn)

    print('Test OK: ' .. debug.getinfo(1, "n").name)
end


local function test_luast_module_destroy()
    print('Test Start: ' .. debug.getinfo(1, "n").name)
    local ret = luast.destroy()

    assert(ret == 0)

    print('Test OK: ' .. debug.getinfo(1, "n").name)
end


local function test_luast_worker_init()
    local ret = luast.worker_init()

    assert(ret == 0)
end


local function test_luast_make_test_suite(stable)
    -- as the type ascending order
    return {
        {
            name = 'bool table op',
            cases = {
                { key = false, value = false },
                { key = true,  value = true },
                { key = true,  value = stable },
            },
        },
        {
            name = 'double table op',
            cases = {
                { key = -1.1, value = -100.1 },
                { key = 1.1,  value = 100.1 },
                { key = 1.2,  value = 100.1 },
                { key = 2.1,  value = stable },
            },
        },
        {
            name = 'str table op',
            cases = {
                { key = '',          value = 'empty string' },
                { key = 'empty key', value = '' },
                { key = 'key',       value = 'value' },
                { key = 'zzz',       value = stable },

            },
        },
        {
            name = 'int table op',
            cases = {
                { key = -1, value = -1 },
                { key = 0,  value = 0 },
                { key = 1,  value = MAX_INT_VALUE },
                { key = 1,  value = MAX_INT_VALUE + 1 },
                { key = 1,  value = MIN_INT_VALUE },
                { key = 1,  value = MIN_INT_VALUE - 1 },
                { key = 2,  value = stable },
            },
        },
    }
end


local function test_luast_table_ops()
    print('    Test Start: ' ..debug.getinfo(1, "n").name)
    local tbl, _, _= luast.new()
    assert(type(tbl) == 'userdata')

    -- non existed key
    assert(nil == tbl[1])

    tbl[2] = 1
    assert(1 == tbl[2])

    tbl[2] = nil
    assert(nil == tbl[2])

    assert(nil == tbl[nil])

    -- test each type
    local suite_info = test_luast_make_test_suite(tbl)

    local function test_table_ops(tbl, key, value, idx)
        -- set
        tbl[key] = value

        -- get
        assert(tbl[key] == value,
               strfmt('case %s failed to set or get', idx))

        -- remove
        tbl[key] = nil
        assert(tbl[key] == nil,
               strfmt('case %s failed to remove', idx))
    end

    local all_keys = {}
    local all_values = {}

    for _, suite in ipairs(suite_info) do
        local msg = strfmt('        - [%s]', suite.name)

        for idx, case in ipairs(suite.cases) do
            local record = strfmt('%s_%d', suite.name, tostring(idx))
            table.insert(all_keys, { name = record, key = case.key })
            table.insert(all_values, { name = record, value = case.value })

            test_table_ops(tbl, case.key, case.value, idx)
        end

        print(strfmt('%s: passed', msg))
    end

    local msg = '        - [all types table op]'
    for _, key_info in ipairs(all_keys) do
        for _, value_info in ipairs(all_values) do
            test_table_ops(tbl,
                           key_info.key,
                           value_info.value,
                           key_info.name .. '-' .. value_info.name)

        end
    end
    print(strfmt('%s passed', msg))

    collectgarbage()
    print('    Test OK: ' ..debug.getinfo(1, "n").name)
end


local function test_luast_iterators()
    print('    Test Start: ' ..debug.getinfo(1, "n").name)
    local stable = luast.new()
    assert(type(stable) == 'userdata')

    assert(#stable == 0, 'empty table length should be 0')

    -- len
    local count = -1000;
    repeat
        stable[count] = count
        if count <= 0 then
            assert(#stable == 0, 'empty array table length should be 0')
        else
            assert(#stable == count, 'wrong table length')
        end

        count = count + 1

    until count == 1001

    stable[10000] = 10000
    stable[1003] = 1003
    assert(10000 == #stable, 'failed to get length of array within holes')

    -- array part
    count = 1
    for idx, val in luast.ipairs(stable) do
        assert(idx == count, 'index value is wrong')
        assert(idx == val, 'failed to iterate array part')

        count = count + 1
        if count == 1001 then
            count = 1003
        elseif count == 1004 then
            count = 10000
        end
    end

    -- dict part
    local dict = luast.new()
    local suite_info = test_luast_make_test_suite(stable)
    local all_cases = {}
    count = 1
    for _, suite in ipairs(suite_info) do
        for _, case in ipairs(suite.cases) do
            local prev = all_cases[count-1]
            if prev ~= nil and prev.key == case.key then
                all_cases[count-1] = case
            else
                all_cases[count] = case
                count = count + 1
            end

            dict[case.key] = case.value

            local num = 1
            for k, v in luast.pairs(dict) do
                assert(k == all_cases[num].key, 'wrong key')
                assert(v == all_cases[num].value, 'wrong value')
                num = num + 1
            end
            assert(num-1 == #all_cases, 'wrong dict length')
        end
    end

    collectgarbage()
    print('    Test OK: ' ..debug.getinfo(1, "n").name)
end


local function test_luast_table_op()
    print('Test Start: ' ..debug.getinfo(1, "n").name)

    -- double init module makes no effect
    luast.module_init(shm_fn)

    local pid = unistd.fork()
    if pid == 0 then
        -- test luast in child process
        test_luast_worker_init()

        test_luast_table_ops()

        test_luast_iterators()

        os.exit(0)
    end

    wait.wait(pid)

    local recycle_num = luast.proc_crash_detection()
    assert(recycle_num == 1, 'failed to recycle crashed process')

    print('Test OK: ' ..debug.getinfo(1, "n").name)
end


local function test_luast_registry_and_crash()
    print('Test Start: ' ..debug.getinfo(1, "n").name)

    -- double module init makes no effect
    luast.module_init(shm_fn)

    local pids = {}

    for idx = 1, 10, 1 do
        local pid = unistd.fork()
        if pid == 0 then
            test_luast_worker_init()

            local self = unistd.getpid()
            local stbl = luast.new()
            stbl[self] = self

            local registry = luast.get_registry()
            registry[self] = stbl

            local mode = idx % 3
            if mode == 0 then
                registry[self] = stbl

            elseif mode == 1 then
                registry[self] = nil

            else
                registry[self] = registry

            end

            os.exit(0)
        else
            table.insert(pids, pid)
        end
    end

    for _, pid in ipairs(pids) do
        wait.wait(pid)
    end

    local recycle_num = luast.proc_crash_detection(10)
    assert(recycle_num == 10, 'failed to recycle all the crashed process')

    for idx, pid in ipairs(pids) do
        local registry = luast.get_registry()
        local stable = registry[pid]
        local mode = idx % 3
        if mode == 0 then
            assert(type(stable) == 'userdata', 'no userdata')
            assert(stable[pid] == pid, 'wrong pid value')

        elseif mode == 1 then
            assert(stable == nil, 'failed to remove table in registry')

        else
            assert(stable == registry, 'failed to refer to registry')
        end
    end

    collectgarbage()
    print('Test OK: ' ..debug.getinfo(1, "n").name)
end


local function test_luast_main()
    test_luast_module_init()

    test_luast_table_op()

    test_luast_registry_and_crash()

    test_luast_module_destroy()

    return 0
end


-- main entry point
test_luast_main()
