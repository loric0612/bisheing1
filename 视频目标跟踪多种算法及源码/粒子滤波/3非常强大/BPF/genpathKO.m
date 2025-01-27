function p = genpathKO(d)
% slightly modified version of genpathKPM() by Kevin M.
% genpathKPM Like built-in genpath, but omits directories whose name is 'Old', 'old' or 'CVS' or 'SVN' 
% function p = genpathKO(d)

if nargin==0,
  p = genpath(fullfile(matlabroot,'toolbox'));
  if length(p) > 1, p(end) = []; end % Remove trailing pathsep
  return
end

% initialise variables
methodsep = '@';  % qualifier for overloaded method directories
p = '';           % path to be returned

% Generate path based on given root directory
files = dir(d);
if isempty(files)
  return
end

% Add d to the path even if it is empty.
p = [p d pathsep];

% set logical vector for subdirectory entries in d
isdir = logical(cat(1,files.isdir));
%
% Recursively descend through directories which are neither
% private nor "class" directories.
%
dirs = files(isdir); % select only directory entries from the current listing

for i=1:length(dirs)
   dirname = dirs(i).name;
   if    ~strcmp( dirname,'.')         & ...
         ~strcmp( dirname,'..')        & ...
         ~strncmp( dirname,methodsep,1)& ...
         ~strcmp( dirname,'private') & ...
     	 ~strcmp( dirname, 'CVS') & ...
		 ~strcmp( dirname, 'SVN') & ...        % added by KO
         ~strcmp( dirname, '.svn') & ...       % added by KO
	 isempty(strfind(dirname, 'Old')) & ...
	 isempty(strfind(dirname, 'old')) 
      p = [p genpathKO(fullfile(d,dirname))]; % recursive calling of this function.
   end
end

%------------------------------------------------------------------------------
