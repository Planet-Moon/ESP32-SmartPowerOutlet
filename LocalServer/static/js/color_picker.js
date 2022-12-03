var color_value;

function apply_color() {
    var color = color_value.toString();
    color = color.replace("rgb(","");
    color = color.replace(")","");
    color = color.split(", ");
    var color_json = {
        "red": Number(color[0]),
        "green": Number(color[1]),
        "blue": Number(color[2])
    };
    var json_color = JSON.stringify(color_json);
    $.ajax({
        method: "POST",
        url: "/led",
        dataType: "json",
        contentType: "application/json; charset=utf-8",
        data: json_color
    });
}

$(function(){
    $('#cp2').on('colorpickerChange colorpickerCreate', function(e){
        color_value = e.value;
    })
});
