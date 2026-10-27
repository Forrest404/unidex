function onEnter()
    print("sensor API:", type(unidex.climate))
end

function draw()
    unidex.header("Environment")

    local reading, err = unidex.climate()

    if not reading then
        unidex.centered(80, "Sensor unavailable")
        unidex.centered(100, err or "Read failed")
    else
        unidex.centered(
            80,
            string.format("%.1f C", reading.temperature_c),
            "large"
        )

        unidex.centered(
            110,
            string.format("%.1f%% RH", reading.humidity_rh),
            "large"
        )
    end

    unidex.hints("Next", "Refresh")
end
