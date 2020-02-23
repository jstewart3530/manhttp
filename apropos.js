const TitleSeparator = "\u2002\u2014\u2002";

const ShowAllBtn = document.getElementById ("ShowAllButton");
const HideAllBtn = document.getElementById ("HideAllButton");
const BySectionBtn = document.getElementById ("BySectionButton");
const ByNameBtn = document.getElementById ("ByNameButton");
const ResultsDiv = document.getElementById ("Results");


let SortMode = null;
let nSections = 0;


let language = navigator.language || "en";
let PageCollator = new Intl.Collator (language, { sensitivity : "case" });
let SectionCollator = new Intl.Collator (language, { sensitivity : "base", numeric : true });



function CompareBySection
   (left, 
    right)

{
  let t = SectionCollator.compare (left [1], right [1]);
  return (t == 0) ? PageCollator.compare (left [0], right [0]) : t;
}


function CompareByName
   (left,
    right)

{
  let t = PageCollator.compare (left [0], right [0]);
  return (t == 0) ? SectionCollator.compare (left [1], right [1]) : t;
}


function ShowSection
   (section,
    state)

{
  let content = document.getElementById (`Section_${section}`);
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

  let button = document.getElementById (`Section_${section}_HideButton`);
  let header = document.getElementById (`Section_${section}_HeaderBar`);

  let v = fShow ? [ "SHOWN", `${content.scrollHeight}px` ]
                : [ "HIDDEN", "0px" ];


  content.style.maxHeight = v [1];
  content.setAttribute ("State", v [0]);
  button.setAttribute ("State", v [0]);
  header.setAttribute ("State", v [0]);
}


function ShowAll
   ()

{
  if (SortMode !== "bysection")
    return;

  for (let i = 0; i < nSections; i++)
  {
    ShowSection (i, "show");
  }
}


function HideAll
   ()

{
  if (SortMode !== "bysection")
    return;

  for (let i = 0; i < nSections; i++)
  {
    ShowSection (i, "hide");
  }
}


function SetSortMode
   (mode)

{
  let fBySection, Compare, LinkText;

  switch (mode)
  {
    case "bysection":   fBySection = true;   break;
    case "byname":      fBySection = false;  break;
    default:            return;
  }

  if (SortMode === mode)
    return;

  SortMode = mode;


  if (fBySection)
  {
    ShowAllBtn.removeAttribute ("Disabled");
    HideAllBtn.removeAttribute ("Disabled");
    ByNameBtn.removeAttribute ("Active");
    BySectionBtn.setAttribute ("Active", "1");

    Compare = CompareBySection;
    LinkText = ((PageName, section) => PageName);
  }
  else
  {
    ShowAllBtn.setAttribute ("Disabled", "1");
    HideAllBtn.setAttribute ("Disabled", "1");
    BySectionBtn.removeAttribute ("Active");
    ByNameBtn.setAttribute ("Active", "1");

    Compare = CompareByName;
    LinkText = ((PageName, section) => `${PageName}(${section})`);
  }


  results.sort (Compare);


  let elts = [... ResultsDiv.children];
  for (let i = elts.length - 1; i >= 0; i--)
  {
    ResultsDiv.removeChild (elts [i]);
  }


  let bar, HideBtn, container, table, row, cell, content;
  let PrevSection = null, nResults = results.length, n = 0;
 
  
  if (!fBySection)
  {
    table = document.createElement ("table"); 
    table.className = "AproposTable";
    ResultsDiv.append (table);
  
    row = document.createElement ("tr");
    row.className = "AproposTableHeader";
    table.append (row);
    
    cell = document.createElement ("th");
    cell.setAttribute ("Column", "page");
    cell.textContent = "Page";
    row.append (cell);
    
    cell = document.createElement ("th");
    cell.setAttribute ("Column", "description");
    cell.textContent = "Description";
    row.append (cell);
  }


  for (let i = 0; i < nResults; i++)
  {
    let [page, section, description] = results [i];    
  

    if (fBySection && (section != PrevSection))
    {
      let ElementID = `Section_${n}`;
      let FullSectionTitle = SectionTitles [section.toLowerCase ()];


      bar = document.createElement ("div");
      bar.id = ElementID + "_HeaderBar";
      bar.className = "AproposSectionBar";
      bar.textContent = (FullSectionTitle && (FullSectionTitle != ""))
                          ? `Section ${section}${TitleSeparator}${FullSectionTitle}`
                          : `Section ${section}`
      bar.setAttribute ("State", "SHOWN");
      ResultsDiv.append (bar);

      HideBtn = document.createElement ("span");
      HideBtn.id = ElementID + "_HideButton";
      HideBtn.className = "HideButton";
      HideBtn.setAttribute ("State", "SHOWN");
      HideBtn.onclick = new Function ("event", `ShowSection (${n}, "toggle");`);
      bar.append (HideBtn);

 
      container = document.createElement ("div");
      container.id = ElementID;
      container.className = "AproposContainer";
      container.style.overflow = "hidden";
      ResultsDiv.append (container);


      table = document.createElement ("table"); 
      table.className = "AproposTable";      
      container.append (table);


      row = document.createElement ("tr");
      row.className = "AproposTableHeader";
      table.append (row);

      row = document.createElement ("tr");
      row.className = "AproposTableHeader";
      table.append (row);

      cell = document.createElement ("th");
      cell.setAttribute ("Column", "page");
      cell.textContent = "Page";
      row.append (cell);
      
      cell = document.createElement ("th");
      cell.setAttribute ("Column", "description");
      cell.textContent = "Description";
      row.append (cell);

      PrevSection = section;
      n++;
    }


    let prefix = (page.indexOf (":") >= 0) ? UriPrefix : "";

    row = document.createElement ("tr");
    row.className = "AproposTableRow";
    table.append (row);

    cell = document.createElement ("td");
    row.append (cell);
    content = document.createElement ("a");
    content.textContent = LinkText (page, section);
    content.setAttribute ("href", `${prefix}man/${page}(${section})`);
    content.setAttribute ("target", "_blank");
    content.setAttribute ("RefType", "manpage");
    cell.append (content);
  
    cell = document.createElement ("td");
    cell.textContent = description;
    row.append (cell);  
  }

  nSections = n;
}


ByNameBtn.onclick = function (event) { SetSortMode ("byname"); };
BySectionBtn.onclick = function (event) { SetSortMode ("bysection"); };
ShowAllBtn.onclick = ShowAll;
HideAllBtn.onclick = HideAll;


SetSortMode ("bysection");


let text = (results.length == 1)
             ? "1 page" : `${results.length} pages in ${nSections} section${(nSections == 1) ? "" : "s"}`;

document.getElementById ("Nav-Apropos-NResults").innerHTML 
      = "<span Label=\"\">Results:</span>" + text;

