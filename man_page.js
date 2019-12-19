const SectionList = document.getElementById ("SectionList");

let idTimeout = -1;



function ShowSection
    (n,
     state)

{
  let idSection = `Sec${n}`;
  let content = document.getElementById (idSection);
  let fCurrentState = (content.getAttribute ("State") != "HIDDEN");
  let fShow;

  switch (state)
  {
    case "show":     fShow = true;            break;
    case "hide":     fShow = false;           break;
    case "toggle":   fShow = !fCurrentState;  break;
    default:         return;
  }

  if (fShow == fCurrentState)
    return;


  let header = document.getElementById (idSection + "_Header");
  let button = document.getElementById (idSection + "_ShowBtn");

  let v = fShow ? [ "SHOWN", `${content.scrollHeight}px`]
                : [ "HIDDEN", "0px" ];


  content.setAttribute ("State", v [0]);
  content.style.maxHeight = v [1];

  header.setAttribute ("State", v [0]);
  button.setAttribute ("State", v [0]);
}



function GoToSection
    (n)

{
  let header = document.getElementById (`Sec${n}_Header`);
  let y = Math.max (0, header.offsetTop - header.clientHeight - 15);

  window.scrollTo ({ top: y, left: 0, behavior: "smooth" });
  
  ShowSection (n, "show");
}


function ShowAll
    (event)

{
  for (let i = 1; i <= nSections; i++)
  {
    ShowSection (i, "show");
  }
}


function HideAll
    (event)

{
  for (let i = 1; i <= nSections; i++)
  {
    ShowSection (i, "hide");
  }
}


function HandleSelection
    (event)

{
  let idSection = +(this.value);       
  if (!idSection || (idSection == NaN) || (idSection <= 0))
    return;

  GoToSection (idSection);


  if (idTimeout > 0)
  {
    window.clearTimeout (idTimeout);
  }

  idTimeout = window.setTimeout
                  (function () {  SectionList.value = "x";  idTimeout = -1;  },
                   3000);
}



for (let e of document.getElementsByClassName ("Collapsible"))
{
  e.setAttribute ("State", "SHOWN");
}

for (let e of document.getElementsByClassName ("HeaderBar"))
{
  e.setAttribute ("State", "SHOWN");
}

for (let e of document.getElementsByClassName ("HideButton"))
{
  e.setAttribute ("State", "SHOWN");
}

document.getElementById ("ShowAllBtn").addEventListener ("click", ShowAll, false);
document.getElementById ("HideAllBtn").addEventListener ("click", HideAll, false);
SectionList.addEventListener ("input", HandleSelection, false);
