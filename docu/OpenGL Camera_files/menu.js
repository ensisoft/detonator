(() =>
{
    // register click events for <a>
    let menu = document.getElementById("menu");
    if(!menu)
        return;

    /*
    let as = menu.querySelectorAll("a");
    as.forEach(a =>
    {
        a.addEventListener("click", () =>
        {
            as.forEach(e => e.classList.remove("focuseditem")); // unfocus all
            a.classList.add("focuseditem");
            sessionStorage.setItem("focusedItem", a.id);        // remember focused
        });
    });
    */
    // focus the prev selection when page is reloaded
    let focusedId = sessionStorage.getItem("focusedItem");
    if(focusedId)
    {
        let a = document.getElementById(focusedId);
        if(a)
            a.classList.add("focuseditem");
    }

    // get prev collapsed <ul>s
    let collapsedUls = JSON.parse(sessionStorage.getItem("collapsedUls"));
    if(!collapsedUls)
    {
        collapsedUls = []; // create new array
    }

    // register click events for <span>
    let plusminus = document.getElementsByClassName("plusminus");
    for(let e of plusminus)
    {
        // get <ul> for curr <span>
        let childUl = e.parentElement.parentElement.querySelector("ul");

        // set and remember height of <ul> for animation
        let height = childUl.scrollHeight;
        if(collapsedUls.includes(childUl.id))
        {
            childUl.style.height = "0px";
            let pm = childUl.parentElement.querySelector("span");
            if(pm)
                pm.classList.add("collapseditem");
        }
        else
        {
            childUl.style.height = height + "px";
        }

        // click event to toggle collapse
        e.addEventListener("click", ev =>
        {
            ev.preventDefault();
            ev.stopPropagation();

            if(e.classList.contains("collapseditem"))
            {
                e.classList.remove("collapseditem");
                childUl.style.height = height + "px";

                let index = collapsedUls.indexOf(childUl.id);
                if(index > -1)
                    collapsedUls.splice(index, 1);
            }
            else
            {
                e.classList.add("collapseditem");
                childUl.style.height = "0px";

                if(!collapsedUls.includes(childUl.id))
                    collapsedUls.push(childUl.id);
            }
            sessionStorage.setItem("collapsedUls", JSON.stringify(collapsedUls));
        });
    }
})();
