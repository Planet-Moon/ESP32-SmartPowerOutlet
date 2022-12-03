$(function(){
    $('#relais_switch').on('input', function(){
        var val = $('#relais_switch')[0]['checked'];

        var relais = {
            "state": val
        };
        var json_relais = JSON.stringify(relais);

        $.ajax({
            method: "POST",
            url: "/relais",
            dataType: "json",
            contentType: "application/json; charset=utf-8",
            data: json_relais
        });
    });
});
