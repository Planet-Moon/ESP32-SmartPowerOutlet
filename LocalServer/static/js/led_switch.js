$(function(){
    $('#led_switch').on('input', function(){
        var val = $('#led_switch')[0]['checked'];

        var led = {
            "on": val
        };
        var json_led = JSON.stringify(led);

        $.ajax({
            method: "POST",
            url: "/led",
            dataType: "json",
            contentType: "application/json; charset=utf-8",
            data: json_led
        });
    });
});
